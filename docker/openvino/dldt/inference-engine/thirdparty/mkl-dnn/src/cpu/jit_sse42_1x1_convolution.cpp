/*******************************************************************************
* Copyright 2017-2018 Intel Corporation
*
* Licensed under the Apache License, Version 2.0 (the "License");
* you may not use this file except in compliance with the License.
* You may obtain a copy of the License at
*
*     http://www.apache.org/licenses/LICENSE-2.0
*
* Unless required by applicable law or agreed to in writing, software
* distributed under the License is distributed on an "AS IS" BASIS,
* WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
* See the License for the specific language governing permissions and
* limitations under the License.
*******************************************************************************/

#include <cstring>
#include "mkldnn_types.h"

#include "c_types_map.hpp"
#include "jit_sse42_1x1_convolution.hpp"
#include "utils.hpp"
#include "mkldnn_thread.hpp"
#include "type_helpers.hpp"

namespace mkldnn {
namespace impl {
namespace cpu {

using namespace mkldnn::impl::status;
using namespace mkldnn::impl::memory_format;
using namespace mkldnn::impl::utils;

template <bool with_relu>
void _jit_sse42_1x1_convolution_fwd_t<with_relu>::execute_forward() {
    auto src = reinterpret_cast<const data_t *>(this->input_memory(0));
    auto weights = reinterpret_cast<const data_t *>(this->input_memory(1));
    auto bias = reinterpret_cast<const data_t *>(this->input_memory(2));
    auto dst = reinterpret_cast<data_t *>(this->memory());

    const memory_desc_wrapper src_d(conf_.src_pd());
    const memory_desc_wrapper dst_d(conf_.dst_pd());
    const memory_desc_wrapper weights_d(conf_.weights_pd(0));

    const auto &jcp = kernel_->jcp;
    int MB = conf_.MB();

    const int work_amount = MB * jcp.ngroups * jcp.nb_bcast;

    if (conf_.want_padded_bias()) {
        for (int oc = 0; oc < jcp.oc_without_padding; ++oc)
            padded_bias_[oc] = bias[oc];
        bias = padded_bias_;
    }

    parallel(0, [&](const int ithr, const int nthr) {
        // TODO (Roma): remove this restriction
        assert(jcp.stride_w == 1 && jcp.stride_h == 1);

        jit_1x1_conv_call_s par_conv = {};

        const int nb_oc = jcp.nb_load;
        const int nb_ic = jcp.nb_reduce;
        const int nb_ic_blocking = jcp.nb_reduce_blocking;
        const int os_block = jcp.bcast_block;

        int start{0}, end{0};
        balance211(work_amount, nthr, ithr, start, end);

        int iwork = start;
        while (iwork < end) {
            int n{0}, g{0}, osb{0};
            nd_iterator_init(iwork, n, MB, g, jcp.ngroups, osb,
                    jcp.nb_bcast);

            const int bcast_step_rem = jcp.nb_bcast - osb;
            int bcast_step = bcast_step_rem <= jcp.nb_bcast_blocking_max
                ? bcast_step_rem : jcp.nb_bcast_blocking;
            bcast_step = nstl::min<int>(bcast_step, end - iwork);

            const int os = osb * os_block;
            const int ow = os % jcp.ow;
            const int oh = os / jcp.ow;
            const int iw = nstl::max<int>(ow * jcp.stride_w - jcp.l_pad, 0);
            const int ih = nstl::max<int>(oh * jcp.stride_h - jcp.t_pad, 0);

            par_conv.bcast_dim = this_block_size(os, jcp.os,
                    bcast_step * os_block);

            int ocb = 0;
            while (ocb < jcp.nb_load) {
                const int load_step_rem = jcp.nb_load - ocb;
                const int load_step = load_step_rem < jcp.nb_load_blocking_max
                    ? load_step_rem : jcp.nb_load_blocking;

                const size_t _ocb = g * nb_oc + ocb;
                par_conv.load_dim = this_block_size(ocb * jcp.oc_block, jcp.oc,
                        load_step * jcp.oc_block);

                const size_t dst_off = dst_d.blk_off(n, _ocb, oh, ow);
                par_conv.output_data = &dst[dst_off];

                par_conv.bias_data = &bias[_ocb * jcp.oc_block];

                for (int icb = 0; icb < nb_ic; icb += nb_ic_blocking) {
                    par_conv.first_last_flag = 0
                        | (icb == 0) * FLAG_REDUCE_FIRST
                        | (icb + nb_ic_blocking >= nb_ic) * FLAG_REDUCE_LAST;

                    par_conv.reduce_dim = this_block_size(icb * jcp.ic_block,
                            jcp.ic, nb_ic_blocking * jcp.ic_block);

                    const size_t _icb = g * nb_ic + icb;
                    const size_t src_off = src_d.blk_off(n, _icb, ih, iw);
                    par_conv.bcast_data = &src[src_off];

                    par_conv.load_data = &weights[conf_.with_groups()
                        ? weights_d.blk_off(g, ocb, icb)
                        : weights_d.blk_off(ocb, icb)];

                    par_conv.oc_off = _ocb * jcp.oc_block * sizeof(float);

                    kernel_->jit_ker(&par_conv);
                }

                ocb += load_step;
            }

            iwork += bcast_step;
        }
    });
}

template <bool with_relu>
void _jit_sse42_1x1_convolution_fwd_t<with_relu>::execute_forward_fusing() {
    auto src = reinterpret_cast<const data_t *>(this->input_memory(0));
    auto weights = reinterpret_cast<const data_t *>(this->input_memory(1));
    auto bias = reinterpret_cast<const data_t *>(this->input_memory(2));
    auto dst = reinterpret_cast<data_t *>(this->memory());

    const memory_desc_wrapper src_d(conf_.src_pd());
    const memory_desc_wrapper weights_d(conf_.weights_pd(0));

    auto &jcp = kernel_->jcp;
    int MB = conf_.MB();

    auto dw_bias = jcp.dw_conv_biases;

    int ocb_work = jcp.with_dw_conv ? utils::div_up(jcp.nb_load, jcp.nb_load_blocking) : 1;
    const int work_amount = MB * jcp.ngroups * ocb_work * jcp.nb_bcast;

    auto step = [](int default_step, int remaining, int tail_step) {
        assert(default_step <= tail_step);
        return remaining < tail_step ? remaining : default_step;
    };

    auto ker = [&](const int ithr, const int nthr) {
        // TODO (Roma): remove this restriction
        assert(jcp.stride_w == 1 && jcp.stride_h == 1);

        auto compute_block_1x1 = [&](float* ws_p, int n, int g, int oh, int ow, int ih, int iw, int os, int os_block, int bcast_step, int ocb, int load_step,
                                    int num_rows) {
            auto p = jit_1x1_conv_call_s();

            for (int h = 0; h < num_rows; h++) {
                ih = nstl::max((oh + h) * jcp.stride_h - jcp.t_pad, 0);

                if ((oh + h) < 0 || (oh + h) >= jcp.ih) {
                    for (int chb = ocb; chb < ocb + load_step; chb++) {
                        memset(ws_p + (((oh + h) + 1) % jcp.dw_conv_ker_h) * jcp.ow * jcp.oc_block +
                               (chb - ocb) * jcp.dw_conv_ker_h * jcp.ow * jcp.oc_block, 0, jcp.ow * jcp.oc_block * sizeof(float));
                    }
                } else {
                    const int _ocb = g * jcp.nb_load + ocb;

                    p.bcast_dim = this_block_size(os, jcp.os, bcast_step * os_block);
                    p.load_dim = this_block_size(ocb * jcp.oc_block, jcp.oc, load_step * jcp.oc_block);

                    p.output_data = &ws_p[(((oh + h) + 1) % jcp.dw_conv_ker_h) * jcp.ow * jcp.oc_block];

                    p.bias_data = &bias[_ocb * jcp.oc_block];

                    for (int icb = 0; icb < jcp.nb_reduce; icb += jcp.nb_reduce_blocking) {
                        p.first_last_flag = 0
                                            | (icb == 0 ? FLAG_REDUCE_FIRST : 0)
                                            | (icb + jcp.nb_reduce_blocking >= jcp.nb_reduce
                                               ? FLAG_REDUCE_LAST : 0);

                        p.reduce_dim = this_block_size(icb * jcp.ic_block, jcp.ic,
                                                       jcp.nb_reduce_blocking * jcp.ic_block);
                        p.load_data = &weights[conf_.with_groups()
                                               ? weights_d.blk_off(g, ocb, icb)
                                               : weights_d.blk_off(ocb, icb)];

                        const int _icb = g * jcp.nb_reduce + icb;
                        p.bcast_data = src + src_d.blk_off(n, _icb, ih, iw);

                        p.oc_off = _ocb * jcp.oc_block * sizeof(float);

                        kernel_->jit_ker(&p);
                    }
                }
            }
        };

        auto compute_row_dw = [&](const float* ws_p, int n, int ocb, int load_step, int dst_idx) {
            const auto &jcp_dw = kernel_dw_->jcp;

            for (int chb = ocb; chb < ocb + load_step; chb++) {
                auto par_conv_dw = jit_conv_call_s();

                par_conv_dw.src_row0 = &ws_p[(((dst_idx+1) - 1) % jcp_dw.kh) * jcp_dw.iw * jcp_dw.ch_block +
                                             (chb - ocb) * jcp_dw.kh * jcp_dw.iw * jcp_dw.ch_block];
                par_conv_dw.src_row1 = &ws_p[(((dst_idx+1) - 0) % jcp_dw.kh) * jcp_dw.iw * jcp_dw.ch_block +
                                             (chb - ocb) * jcp_dw.kh * jcp_dw.iw * jcp_dw.ch_block];
                par_conv_dw.src_row2 = &ws_p[(((dst_idx+1) + 1) % jcp_dw.kh) * jcp_dw.iw * jcp_dw.ch_block +
                                             (chb - ocb) * jcp_dw.kh * jcp_dw.iw * jcp_dw.ch_block];

                par_conv_dw.dst = &dst[n*jcp_dw.oc*jcp_dw.oh*jcp_dw.ow + chb*jcp_dw.ch_block*jcp_dw.oh*jcp_dw.ow +
                                       dst_idx/jcp_dw.stride_h*jcp_dw.ow*jcp_dw.ch_block];

                par_conv_dw.kh_padding = jcp_dw.kh;
                par_conv_dw.filt = &jcp.dw_conv_weights[chb * jcp_dw.kh * jcp_dw.kw * jcp_dw.ch_block];
                par_conv_dw.bias = &dw_bias[chb * jcp_dw.ch_block];
                par_conv_dw.ur_w = (size_t)(jcp_dw.ow);

                kernel_dw_->jit_ker(&par_conv_dw);
            }
        };

        assert(jcp.stride_w == 1 && jcp.stride_h == 1);

        int start{0}, end{0};
        balance211(work_amount, nthr, ithr, start, end);

        auto pbuf = dw_conv_buffer_ + ithr * dw_conv_buffer_size_;

        const int os_block = jcp.iw;


        int iwork = start;
        while (iwork < end) {
            int n{0}, g{0}, ocbb{0}, osb{0};
            nd_iterator_init(iwork, n, MB, g, jcp.ngroups, ocbb, ocb_work, osb,
                             jcp.nb_bcast);

            int bcast_step = 1;

            const int os = osb * os_block;
            const int oh = os / jcp.ow;
            const int ow = os % jcp.ow;

            const int ih = nstl::max(oh * jcp.stride_h - jcp.t_pad, 0);
            const int iw = nstl::max(ow * jcp.stride_w - jcp.l_pad, 0);

            int ocb = ocbb * jcp.nb_load_blocking;

            const int load_step = step(jcp.nb_load_blocking,
                                       jcp.nb_load - ocb, jcp.nb_load_blocking_max);

            if (iwork == start || oh == 0) {
                bcast_step = nstl::min(1, end - iwork);
                compute_block_1x1(pbuf, n, g, oh - 1, ow, ih, iw, os, os_block, bcast_step, ocb, load_step, bcast_step + 2);
            } else {
                bcast_step = nstl::min(1, end - iwork);
                compute_block_1x1(pbuf, n, g, oh + 1, ow, ih, iw, os, os_block, bcast_step, ocb, load_step, bcast_step);
            }

            if ((oh % jcp.dw_conv_str_h == 0)) {
                compute_row_dw(pbuf, n, ocb, load_step, oh);
            }

            iwork += bcast_step;
        }
    };

    if (conf_.want_padded_bias()) {
        for (int oc = 0; oc < jcp.oc_without_padding; ++oc)
            padded_bias_[oc] = bias[oc];
        bias = padded_bias_;

        for (int oc = 0; oc < jcp.oc_without_padding; ++oc)
            dw_padded_bias_[oc] = dw_bias[oc];
        dw_bias = dw_padded_bias_;
    }

    parallel(0, ker);
}

template void _jit_sse42_1x1_convolution_fwd_t<true>::execute_forward();
template void _jit_sse42_1x1_convolution_fwd_t<false>::execute_forward();
template void _jit_sse42_1x1_convolution_fwd_t<true>::execute_forward_fusing();
template void _jit_sse42_1x1_convolution_fwd_t<false>::execute_forward_fusing();

}
}
}

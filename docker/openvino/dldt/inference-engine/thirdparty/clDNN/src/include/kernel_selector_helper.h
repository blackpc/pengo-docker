// Copyright (c) 2016-2018 Intel Corporation
//
// Licensed under the Apache License, Version 2.0 (the "License");
// you may not use this file except in compliance with the License.
// You may obtain a copy of the License at
//
//      http://www.apache.org/licenses/LICENSE-2.0
//
// Unless required by applicable law or agreed to in writing, software
// distributed under the License is distributed on an "AS IS" BASIS,
// WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
// See the License for the specific language governing permissions and
// limitations under the License.

#pragma once

#include "api/C/cldnn.h"
#include "api/CPP/program.hpp"

#include "gpu/ocl_toolkit.h"
#include "program_impl.h"

#include "kernel_selector_params.h"
#include "kernel_selector_common.h"
#include "tensor_type.h"

#include <cstdint>

using namespace cldnn;

namespace kernel_selector
{
    using n_dims                            = kernel_selector::Tensor::NDims;
    using kernel_data                       = kernel_selector::KernelData;
    using kernel_string                     = kernel_selector::KernelString;
    using cl_kernel_data                    = kernel_selector::clKernelData;
    using kernel_arguments                  = kernel_selector::Arguments;
    using kernel_argument_element           = kernel_selector::ArgumentDescriptor;
    using kernel_argument_types             = kernel_selector::ArgumentDescriptor::Types;
    using kernel_scalar_arguments           = kernel_selector::Scalars;
    using kernel_scalar_argument_types      = kernel_selector::ScalarDescriptor::Types;

    using data_type                         = kernel_selector::Datatype;
    using kernel_type                       = kernel_selector::KernelType;
    using weights_type                      = kernel_selector::WeightsType;
    using activation_function               = kernel_selector::ActivationFunction;
    using pool_type                         = kernel_selector::PoolType;
    using pool_remainder                    = kernel_selector::PoolRemainder;
	using argm_axis                         = kernel_selector::ArgMaxMinAxis;
	using argm_output                       = kernel_selector::ArgMaxMinOut;
    using lookt_axis                        = kernel_selector::LookUpTableAxis;
    using lrn_mode                          = kernel_selector::LRNMode;
    using normalize_mode                    = kernel_selector::NormalizeMode;
    using mvn_mode                          = kernel_selector::MVNMode;
    using kernel_divider_mode               = kernel_selector::KernelDividerMode;
    using eltwise_mode                      = kernel_selector::EltwiseMode;
    using eltwise_input_mode                = kernel_selector::EltwiseInputMode;
    using softmax_dim                       = kernel_selector::SoftmaxDim;
    using mean_subtruct_mode                = kernel_selector::MeanSubtractMode;
    using mean_op                           = kernel_selector::MeanOp;
    using concat_axis                       = kernel_selector::ConcatAxis;
    using tile_axis                         = kernel_selector::TileAxis;
    using tuning_mode                       = kernel_selector::TuningMode;
    using sample_type                       = kernel_selector::SampleType;
    using border_type                       = kernel_selector::BorderType;

    using data_tensor                       = kernel_selector::DataTensor;
    using weights_tensor                    = kernel_selector::WeightsTensor;
    template <typename T> using dim_tensor  = kernel_selector::DimTensor<T>;
    using data_layout                       = kernel_selector::DataLayout;
    using weights_layout                    = kernel_selector::WeightsLayout;
    using multi_data_tensor                 = kernel_selector::MultiDataTensor;

    using params                            = kernel_selector::Params;
    using weights_reorder_params            = kernel_selector::WeightsReorderParams;
    using generic_kernel_params             = kernel_selector::GenericKernelParams;
}

kernel_selector::data_type to_data_type(data_types dt);
data_types from_data_type(kernel_selector::data_type dt);
kernel_selector::weights_type to_weights_type(data_types dt);
data_types from_weights_type(kernel_selector::weights_type dt);
kernel_selector::data_layout to_data_layout(format f);
cldnn::format from_data_layout(kernel_selector::data_layout l);
kernel_selector::weights_layout to_weights_layout(format f);
cldnn::format::type from_weights_layout(kernel_selector::weights_layout l);
kernel_selector::tuning_mode to_tuning_mode(cldnn::tuning_mode mode);
std::string to_host_version(const cldnn::version_t& version);
kernel_selector::data_tensor convert_data_tensor(const layout& l, uint32_t split = 1, const tensor view_offset = {});
kernel_selector::weights_tensor convert_weights_tensor(const layout& l);
kernel_selector::activation_function get_kernel_selector_activation_param(cldnn_activation_func activation_func);
kernel_selector::activation_function get_kernel_selector_activation_grad_param(cldnn_activation_grad_func activation_grad_func);

template <typename T = std::uint32_t>
kernel_selector::dim_tensor<T> convert_dim_vector(const tensor& t)
{
    const auto& sizes = t.sizes(format::bfyx);
    return {
        static_cast<T>(sizes[0]),
        static_cast<T>(sizes[1]),
        static_cast<T>(sizes[2]),
        static_cast<T>(sizes[3]),
    };
}

template <typename p_type>
inline void convert_activation_func_params(const p_type primitive, kernel_selector::base_params& params)
{
    const float negative_slope = primitive->activation_negative_slope;
    if (negative_slope != 0.0f)
    {
        params.activationParams.m = negative_slope;
        params.activationFunc = kernel_selector::activation_function::RELU_NEGATIVE_SLOPE;
    }
    else
    {
        params.activationFunc = kernel_selector::activation_function::RELU;
    }
}

template <typename arg_t>
inline void convert_fused_activation_func_params(const arg_t& arg, kernel_selector::base_params& params)
{
    params.activationParams.m = arg.get_fused_activation_params().a;
    params.activationParams.n = arg.get_fused_activation_params().b;
    params.activationFunc = get_kernel_selector_activation_param(arg.get_fused_activation_func());
}

template <typename p_type>
inline void convert_new_activation_func(const p_type primitive, kernel_selector::base_params& params)
{
    params.activationFunc = get_kernel_selector_activation_param(primitive->activation_func);
    params.activationParams.m = primitive->additional_params.a;
    params.activationParams.n = primitive->additional_params.b;
}

template <typename params_t, typename arg_t>
inline params_t get_default_params(const arg_t& arg, uint32_t split = 1)
{
    params_t params;

    const auto& context = arg.get_program().get_engine().get_context();
    const auto& engine_info = context->get_engine_info();

    params.engineInfo.bSubGroupSupport      = context->extension_supported("cl_intel_subgroups");
    params.engineInfo.bSubGroupShortSupport = context->extension_supported("cl_intel_subgroups_short");
    params.engineInfo.bFP16Support          = context->extension_supported("cl_khr_fp16");
    params.engineInfo.bFP64Support          = context->extension_supported("cl_khr_fp64");
    params.engineInfo.bIMADSupport          = engine_info.supports_imad != 0;
    params.engineInfo.bIMMADSupport         = engine_info.supports_immad != 0;
    params.engineInfo.bImageSupport         = engine_info.supports_image != 0;
    params.engineInfo.maxWorkGroupSize      = engine_info.max_work_group_size;
    params.engineInfo.maxLocalMemSize       = engine_info.max_local_mem_size;
    params.engineInfo.maxImage2dWidth       = engine_info.max_image2d_width;
    params.engineInfo.maxImage2dHeight      = engine_info.max_image2d_height;
    params.engineInfo.deviceId              = engine_info.dev_id;
    params.engineInfo.driverVersion         = engine_info.driver_version;
    params.engineInfo.hostVersion           = to_host_version(cldnn::get_version());
    
    const auto& input_layout    = arg.input().get_output_layout();
    const auto& output_layout   = arg.get_output_layout();

    params.inputs[0] = convert_data_tensor(input_layout, split);
    params.output = convert_data_tensor(output_layout, split);

    params.layerID = arg.id();

    convert_fused_activation_func_params(arg, params);

    return params;
}

template <typename params_t, typename arg_t>
inline params_t get_weights_bias_default_params(const arg_t& arg, uint32_t split = 1)
{
    params_t params = get_default_params<params_t>(arg, split);

    const auto& weights_layout = arg.weights().get_output_layout();
    params.weights = convert_weights_tensor(weights_layout);

    if (arg.bias_term())
    {
        const auto& bias_layout = arg.bias().get_output_layout();
        // bias per output is not supported on cldnn
        params.bias.push_back(convert_data_tensor(bias_layout).FlattenFeatureAndSpatials());
    }

    return params;
}

template <typename params_t, typename arg_t>
inline params_t get_default_learning_params(const arg_t& arg, uint32_t split = 1)
{
	params_t params = get_weights_bias_default_params<params_t>(arg, split);

	const auto learning_params = arg.get_program().get_options().template get<build_option_type::learning_config>()->params;

	if (arg.use_momentum())
	{
		params.use_momentum = true;
	}

	params.momentum_factor = learning_params.momentum;
	params.weights_decay = learning_params.weights_decay;

	return params;
}

template <typename optional_params_t>
inline optional_params_t get_default_optional_params(const program_impl& program)
{
    optional_params_t params;
    
    const auto& context = program.get_engine().get_context();

    params.meaningfulKernelsNames       = context->get_configuration().meaningful_kernels_names;
    params.allowStaticInputReordering   = program.get_options().get<build_option_type::optimize_data>()->enabled();
    params.allowInputReordering         = false;
    params.allowOutputReordering        = false;
    
    const auto& tuning_config = program.get_options().get<build_option_type::tuning_config>();
    params.tuningParams.mode = to_tuning_mode(tuning_config->config.mode);
    params.tuningParams.cacheFilePath = tuning_config->config.cache_file_path;

    return params;
}

template <typename optional_params_t>
inline optional_params_t get_default_weights_bias_optional_params(const program_impl& program)
{
    return get_default_optional_params<optional_params_t>(program);
}

template <typename optional_params_t>
inline optional_params_t get_default_learning_optional_params(const program_impl& program)
{
	return get_default_weights_bias_optional_params<optional_params_t>(program);
}

// Copyright (c) 2018 Intel Corporation
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


#include "broadcast_inst.h"

#include "primitive_gpu_base.h"
#include "implementation_map.h"
#include "kernel_selector_helper.h"
#include "broadcast/broadcast_kernel_selector.h"
#include "broadcast/broadcast_kernel_base.h"
#include "error_handler.h"

namespace cldnn { namespace gpu {

struct broadcast_gpu : typed_primitive_gpu_impl<broadcast>
{
    using parent = typed_primitive_gpu_impl<broadcast>;
    using parent::parent;


    static primitive_impl* create(const broadcast_node& arg)
    { 
        auto bc_params          = get_default_params<kernel_selector::broadcast_params>(arg, 1);
        auto bc_optional_params = get_default_optional_params<kernel_selector::broadcast_optional_params>(arg.get_program());

        auto& kernel_selector = kernel_selector::broadcast_kernel_selector::Instance();
        auto best_kernels = kernel_selector.GetBestKernels(bc_params, bc_optional_params);

        CLDNN_ERROR_BOOL(arg.id(), "Best_kernel.empty()", best_kernels.empty(), "Cannot find a proper kernel with this arguments");

        return new broadcast_gpu(arg, best_kernels[0]);
    }
};

namespace {
    struct attach {
        attach() {
            auto val_fw = broadcast_gpu::create;

            implementation_map<broadcast>::add(std::make_tuple(engine_types::ocl, data_types::f32, format::yxfb), val_fw);
            implementation_map<broadcast>::add(std::make_tuple(engine_types::ocl, data_types::f16, format::yxfb), val_fw);
            implementation_map<broadcast>::add(std::make_tuple(engine_types::ocl, data_types::i8,  format::yxfb), val_fw);
            implementation_map<broadcast>::add(std::make_tuple(engine_types::ocl, data_types::u8,  format::yxfb), val_fw);

            implementation_map<broadcast>::add(std::make_tuple(engine_types::ocl, data_types::f32, format::bfyx), val_fw);
            implementation_map<broadcast>::add(std::make_tuple(engine_types::ocl, data_types::f16, format::bfyx), val_fw);
            implementation_map<broadcast>::add(std::make_tuple(engine_types::ocl, data_types::i8,  format::bfyx), val_fw);
            implementation_map<broadcast>::add(std::make_tuple(engine_types::ocl, data_types::u8,  format::bfyx), val_fw);

            implementation_map<broadcast>::add(std::make_tuple(engine_types::ocl, data_types::f32, format::byxf), val_fw);
            implementation_map<broadcast>::add(std::make_tuple(engine_types::ocl, data_types::f16, format::byxf), val_fw);
            implementation_map<broadcast>::add(std::make_tuple(engine_types::ocl, data_types::i8,  format::byxf), val_fw);
            implementation_map<broadcast>::add(std::make_tuple(engine_types::ocl, data_types::u8,  format::byxf), val_fw);
        }
        ~attach() = default;
    };

    attach attach_impl;

}
} }

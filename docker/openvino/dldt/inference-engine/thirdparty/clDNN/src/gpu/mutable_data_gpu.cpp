/*
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
*/

#include "mutable_data_inst.h"
#include "primitive_gpu_base.h"
#include "implementation_map.h"

namespace cldnn { namespace gpu {

struct mutable_data_gpu : public typed_primitive_gpu_impl<mutable_data>
{
    using parent = typed_primitive_gpu_impl<mutable_data>;
    using parent::parent;

public:

    static primitive_impl* create(mutable_data_node const& arg) 
    { 
        return new mutable_data_gpu(arg, {});
    }
};

namespace {
    struct attach {
        attach() {
            implementation_map<mutable_data>::add({
                { engine_types::ocl, mutable_data_gpu::create }
            });
        }
        ~attach() {}
    };
    attach attach_impl;
}
} }

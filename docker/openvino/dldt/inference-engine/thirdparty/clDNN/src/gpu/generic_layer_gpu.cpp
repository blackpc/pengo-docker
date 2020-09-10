/*
// Copyright (c) 2016 Intel Corporation
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

#include "generic_layer_inst.h"
#include "kernel.h"
#include "implementation_map.h"
#include "kernel_selector_helper.h"
#include "network_impl.h"
#include "engine_impl.h"

using namespace cldnn;

namespace neural
{

struct generic_layer_gpu : typed_primitive_impl<generic_layer>
{
    const generic_layer_node& outer;
    const kernel_selector::cl_kernel_data& _cl_kernel_data;
    gpu::kernel _kernel;

    generic_layer_gpu(const generic_layer_node& arg)
    : outer(arg)
    , _cl_kernel_data(*outer.get_primitive()->generic_params.clKernel.get())
    , _kernel(arg.get_program().get_engine().get_context(), outer.get_primitive()->generic_params.clKernel->kernelString)
    {}

    event_impl::ptr execute_impl(const std::vector<event_impl::ptr>& events, generic_layer_inst& instance) override
    {
        gpu::kernel::kernel_arguments_data args;
        args.scalars = &_cl_kernel_data.scalars;

        for (size_t i = 0; i < instance.inputs_memory_count(); i++)
        {
            args.inputs.push_back(&instance.input_memory(i));
        }
        args.output = &instance.output_memory();
        _kernel.set_output_event(instance.node.is_output());
        return _kernel.run(_cl_kernel_data, events, args);
    }
};

// TODO: move this file to cpu folder and add a new traget to 'cldnn::engine_types'
struct generic_layer_cpu : typed_primitive_impl<generic_layer>
{
    const generic_layer_node& outer;

    generic_layer_cpu(const generic_layer_node& arg) : outer(arg) {}

    event_impl::ptr execute_impl(const std::vector<event_impl::ptr>& events, generic_layer_inst& instance) override
    {
        auto& input_mem = instance.input_memory();
        auto& output_mem = instance.output_memory();

        std::vector<event_impl::ptr> tmp_events(events);

        for (auto& a : events) {
            a->wait();
        }

        mem_lock<uint8_t> old_pointer(input_mem);
        mem_lock<uint8_t> new_pointer(output_mem);

        const auto& cpu_kernel = *outer.get_primitive()->generic_params.cpuKernel.get();

        cpu_kernel.Execute(old_pointer.data(), old_pointer.size(), new_pointer.data(), new_pointer.size());

        return instance.get_network().get_engine().create_user_event(true);
    }
};

static primitive_impl* create(const generic_layer_node& arg)
{
    if (arg.get_primitive()->generic_params.engine == kernel_selector::generic_kernel_params::Engine::GPU)
    {
        return new generic_layer_gpu(arg);
    }
    else
    {
        return new generic_layer_cpu(arg);
    }
}

namespace {
    struct attach {
        attach() {
            implementation_map<generic_layer>::add({
                { cldnn::engine_types::ocl, create }
            });
        }
        ~attach() {}
    };
    attach attach_impl;
}
}
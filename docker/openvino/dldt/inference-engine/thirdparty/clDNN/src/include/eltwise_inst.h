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

///////////////////////////////////////////////////////////////////////////////////////////////////
#pragma once
#include "api/CPP/eltwise.hpp"
#include "primitive_inst.h"
#include <memory>
#include "topology_impl.h"

namespace cldnn
{
template <>
struct typed_program_node<eltwise> : public typed_program_node_base<eltwise>
{
    using parent = typed_program_node_base<eltwise>;

public:
    typed_program_node(std::shared_ptr<primitive> prim, program_impl& prog)
        :parent(prim, prog)
        , output_qf(get_primitive()->output_quantization_factor)
        , output_cf(!get_primitive()->output_calibration_factors.empty())
    {
    }


    program_node& input(size_t idx = 0) const { return get_dependency(idx); }
    size_t inputs_count() const { return get_dependencies().size() - (output_cf ? 1 : 0); }
    program_node& output_calibration_factors() const { return get_dependency(inputs_count()); }
    bool output_calibration_term() const { return !get_primitive()->output_calibration_factors.empty(); }
    float get_output_qf() const { return output_qf; }

private:
    float output_qf;
    bool output_cf; // to know if we have calibration factors
};

using eltwise_node = typed_program_node<eltwise>;

template <>
class typed_primitive_inst<eltwise> : public typed_primitive_inst_base<eltwise>
{
    using parent = typed_primitive_inst_base<eltwise>;

public:
    static layout calc_output_layout(eltwise_node const& node);
    static std::string to_string(eltwise_node const& node);

public:
    typed_primitive_inst(network_impl& network, eltwise_node const& node);

    memory_impl& output_calibration_factors_memory() const { return dep_memory(node.inputs_count()); } // because last place should be reserved for calibration factors
    bool output_calibration_factors_term() const { return node.output_calibration_term(); }
};

using eltwise_inst = typed_primitive_inst<eltwise>;

}

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
#include "../C/batch_norm.h"
#include "primitive.hpp"

namespace cldnn
{
/// @addtogroup cpp_api C++ API
/// @{
/// @addtogroup cpp_topology Network Topology
/// @{
/// @addtogroup cpp_primitives Primitives
/// @{

/// @brief Batch normalization primitive.
/// @details Performs batch normalization as discribed in
/// "Batch Normalization: Accelerating Deep Network Training by Reducing Internal Covariate Shift" by Ioffe, Szegedy
/// @n See: http://arxiv.org/abs/1502.03167
/// 
/// <b>Algorithm:</b>
/// @n global stats can be computed as:
/// @n out[i] = (in[i] - mean[b]) / sqrt(variance[b] + epsilon)

struct batch_norm : public primitive_base<batch_norm, CLDNN_PRIMITIVE_DESC(batch_norm)>
{
    CLDNN_DECLARE_PRIMITIVE(batch_norm)

    /// @brief Constructs batch normalization primitive.
    /// @param id This primitive id.
    /// @param input Input primitive id.
    /// @param mean Primitive id containing mean data.
    /// @param variance Primitive id containing variance.
    /// @param epsilon Epsilon.
    batch_norm(
        const primitive_id& id,
        const primitive_id& input,
        const primitive_id& mean,
        const primitive_id& variance,
        float epsilon,
        const padding& output_padding = padding()
    )
        :primitive_base(id, {input}, output_padding)
        , mean(mean)
        , variance(variance)
        , inv_variance("")
        , epsilon(epsilon)
    {
    }

    /// @brief Constructs batch normalization primitive with mean and variance calculation (used for training).
    /// @param id This primitive id.
    /// @param input Input primitive id.
    /// @param epsilon Epsilon.
    /// @param inv_variance Primitive id containing inverted variance calculated in this primitive. For inference leave empty.
    batch_norm(
        const primitive_id& id,
        const primitive_id& input,
        float epsilon,
        const primitive_id& inv_variance = "",
        const padding& output_padding = padding()
    )
        :primitive_base(id, { input }, output_padding)
        , mean("")
        , variance("")
        , inv_variance(inv_variance)
        , epsilon(epsilon)
    {
    }

    /// @brief Constructs a copy from C API @CLDNN_PRIMITIVE_DESC{batch_norm}
    batch_norm(const dto* dto)
        :primitive_base(dto)
        , mean(dto->mean)
        , variance(dto->variance)
        , inv_variance(dto->inv_variance)
        , epsilon(dto->epsilon)
    {
    }

    /// @brief Primitive id containing mean data.
    primitive_id mean;
    /// @brief Primitive id containing variance.
    primitive_id variance;
    /// @brief Primitive id containing inverted variance used in future gradient computing.
    primitive_id inv_variance;
    /// @brief Epsilon.
    float epsilon;

protected:
    std::vector<std::reference_wrapper<const primitive_id>> get_dependencies() const override 
    { 
        if (!mean.empty() && !variance.empty())
            return{ mean, variance };
        else if (!inv_variance.empty())
            return{ inv_variance };
        else
            return{};
    }

    void update_dto(dto& dto) const override
    {
        dto.mean = mean.c_str();
        dto.variance = variance.c_str();
        dto.inv_variance = inv_variance.c_str();
        dto.epsilon = epsilon;
    }
};
/// @}
/// @}
/// @}
}

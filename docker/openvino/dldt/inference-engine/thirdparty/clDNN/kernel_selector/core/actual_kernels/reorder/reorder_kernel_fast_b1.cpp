﻿/*
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

#include "reorder_kernel_fast_b1.h"
#include "kernel_selector_utils.h"
 
namespace kernel_selector 
{
    ParamsKey ReorderKernelFastBatch1::GetSupportedKey() const
    {
        ParamsKey k;
        k.EnableInputDataType(Datatype::F16);
        k.EnableInputDataType(Datatype::F32);
        k.EnableOutputDataType(Datatype::F16);
        k.EnableOutputDataType(Datatype::F32);
        k.EnableDifferentTypes();
        k.EnableAllInputLayout();
        k.EnableAllOutputLayout();
        k.EnableTensorOffset();
        k.EnableTensorPitches();
        k.EnableBatching();
        return k;
    }

    JitConstants ReorderKernelFastBatch1::GetJitConstants(const reorder_params& params) const
    {
        auto jit = ReorderKernelBase::GetJitConstants(params);
        jit.Merge(GetTensorFriendlyWorkGroupsJit(params.inputs[0]));

        KernelData kd = KernelData::Default<reorder_params>(params);
        reorder_params& newParams = *static_cast<reorder_params*>(kd.params.get());

        const auto& input = newParams.inputs[0];
        jit.AddConstant(MakeJitConstant("ELEMENTS_COUNT", input.LogicalSize()));

        const auto& output = newParams.output;
        if (input.GetLayout() == output.GetLayout() && input.SameDimsSizes(output) &&
            !input.PitchesDifferFromLogicalDims() && !output.PitchesDifferFromLogicalDims() &&
            input.GetDType() != output.GetDType() &&
            params.mode == MeanSubtractMode::NONE)
        {
            jit.AddConstant(MakeJitConstant("CHANGE_DATA_TYPE_ONLY", 1));
        }

        return jit;
    }

    ReorderKernelFastBatch1::DispatchData ReorderKernelFastBatch1::SetDefault(const reorder_params& params) const
    {
        DispatchData kd;

        const auto& input = params.inputs[0];

        unsigned int gws = (unsigned int)input.LogicalSize();

        kd.gws0 = Align(gws, 32);
        kd.gws1 = 1;
        kd.gws2 = 1;

        kd.lws0 = 32;
        kd.lws1 = 1;
        kd.lws2 = 1;

        return kd;
    }

    KernelsData ReorderKernelFastBatch1::GetKernelsData(const Params& params, const optional_params& options) const
    {
        assert(params.GetType() == KernelType::REORDER);

        const reorder_params& orgParams = static_cast<const reorder_params&>(params);

        const auto& input = orgParams.inputs[0];
        const auto& output = orgParams.output;

        auto estimatedTime = DONT_USE_IF_HAVE_SOMETHING_ELSE;

        if (input.Batch().v == 1 && output.Batch().v == 1)
            estimatedTime = FORCE_PRIORITY_6;

        return GetCommonKernelsData(orgParams, options, estimatedTime);
    }
}
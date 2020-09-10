// Copyright (C) 2018 Intel Corporation
//
// SPDX-License-Identifier: Apache-2.0
//

#pragma once

#include <ie_common.h>
#include <mkldnn_node.h>
#include <memory>
#include <string>
#include <vector>

namespace MKLDNNPlugin {

class MKLDNNFullyConnectedNode : public MKLDNNNode {
public:
    MKLDNNFullyConnectedNode(const InferenceEngine::CNNLayerPtr& layer, const mkldnn::engine& eng);
    ~MKLDNNFullyConnectedNode() override = default;

    void getSupportedDescriptors() override;
    void createPrimitive() override;
    bool created() const override;
    bool canBeInPlace() const override {
        return false;
    }

    const std::vector<impl_desc_type>& getPrimitivesPriority() override;
    void createDescriptor(const std::vector<InferenceEngine::TensorDesc>& inputDesc,
                          const std::vector<InferenceEngine::TensorDesc>& outputDesc) override;

private:
    static Register<MKLDNNFullyConnectedNode> reg;
    InferenceEngine::SizeVector weightsDims;
    InferenceEngine::SizeVector biasesDims;
    mkldnn::memory::format weightsFormatForSrcFormat(mkldnn::memory::format sourceFormat);
};

}  // namespace MKLDNNPlugin


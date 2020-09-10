// Copyright (C) 2018 Intel Corporation
//
// SPDX-License-Identifier: Apache-2.0
//

#pragma once

#include <ie_common.h>
#include <mkldnn_node.h>
#include <string>
#include <memory>
#include <vector>

namespace MKLDNNPlugin {

class MKLDNNLrnNode : public MKLDNNNode {
public:
    MKLDNNLrnNode(const InferenceEngine::CNNLayerPtr& layer, const mkldnn::engine& eng);
    ~MKLDNNLrnNode() override = default;

    void getSupportedDescriptors() override;
    void initOptimalPrimitiveDescriptor() override;
    void createDescriptor(const std::vector<InferenceEngine::TensorDesc>& inputDesc,
                          const std::vector<InferenceEngine::TensorDesc>& outputDesc) override;
    void createPrimitive() override;
    bool created() const override;
    bool canBeInPlace() const override {
        return false;
    }

private:
    static Register<MKLDNNLrnNode> reg;
    bool isAcrossMaps;
    int size;
    int k;
    float alpha;
    float beta;
};

}  // namespace MKLDNNPlugin


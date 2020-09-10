// Copyright (C) 2018 Intel Corporation
//
// SPDX-License-Identifier: Apache-2.0
//

#include <mkldnn_extension_mngr.h>
#include <mkldnn_extension_utils.h>
#include "mkldnn_generic_node.h"
#include <vector>
#include <string>
#include <blob_factory.hpp>

using namespace mkldnn;
using namespace MKLDNNPlugin;

MKLDNNGenericNode::MKLDNNGenericNode(const InferenceEngine::CNNLayerPtr& layer, const mkldnn::engine& eng) : MKLDNNNode(layer, eng) {}

void MKLDNNGenericNode::getSupportedDescriptors() {
    if (!extFactory) {
        std::string type = getCnnLayer() ? getCnnLayer()->type : "Generic";
        THROW_IE_EXCEPTION << "Cannot get generic primitive for layer: " << getName() << " with type: " << type;
    }
}

void MKLDNNGenericNode::initSupportedPrimitiveDescriptors() {
    if (!supportedPrimitiveDescriptors.empty())
        return;

    InferenceEngine::Precision precision = getCnnLayer()->insData[0].lock()->getPrecision();
    if (precision != InferenceEngine::Precision::FP32)
        precision = InferenceEngine::Precision::FP32;
    auto inputDataType = MKLDNNExtensionUtils::IEPrecisionToDataType(precision);
    precision = getCnnLayer()->outData[0]->getPrecision();
    if (precision != InferenceEngine::Precision::FP32)
        precision = InferenceEngine::Precision::FP32;
    auto outputDataType = MKLDNNExtensionUtils::IEPrecisionToDataType(precision);

    if (!extFactory)
        THROW_IE_EXCEPTION << "Descriptor for generic primitive doesn't exist";

    InferenceEngine::ResponseDesc resp;
    InferenceEngine::StatusCode rc = extFactory->getImplementations(impls, &resp);
    if (rc != InferenceEngine::OK) {
        THROW_IE_EXCEPTION << resp.msg;
    }
    for (auto &impl : impls) {
        std::vector<InferenceEngine::LayerConfig> configs;
        rc = impl->getSupportedConfigurations(configs, &resp);
        if (rc != InferenceEngine::OK) {
            THROW_IE_EXCEPTION << resp.msg;
        }

        for (auto& config : configs) {
            supportedPrimitiveDescriptors.emplace_back(config, impl_desc_type::unknown);
        }
    }
    if (impls.empty()) {
        THROW_IE_EXCEPTION << "Layer " << getName() << " hasn't available configurations!";
    }
}

void MKLDNNGenericNode::createPrimitive() {
    if (extFactory) {
        return;
    }
    if (getSelectedPrimitiveDescriptor() == nullptr)
        THROW_IE_EXCEPTION << "Preferable primitive descriptor does not set.";
}

void MKLDNNGenericNode::execute(mkldnn::stream strm) {
    if (!impls.empty()) {
        execLayer();
    } else {
        THROW_IE_EXCEPTION << "Descriptor for generic primitive doesn't exist";
    }
}

bool MKLDNNGenericNode::created() const {
    return Generic == getType();
}

bool MKLDNNGenericNode::created(const MKLDNNExtensionManager::Ptr &extMgr) {
    if (getCnnLayer() && extMgr) {
        // We should save extension manager in otder to avoid situation when
        // it will destroyed before extensibility primitives
        extensionManager = extMgr;
        extFactory.reset(extensionManager->CreateExtensionFactory(getCnnLayer()));

        if (extFactory)
            setType(Generic);
    }
    return created();
}

void MKLDNNGenericNode::cleanup() {
    MKLDNNNode::cleanup();
    extFactory.reset();
}

void MKLDNNGenericNode::execLayer() {
    bool isDynBatch = dynBatchLim > 0;
    std::vector<InferenceEngine::Blob::Ptr> inputs;
    std::vector<InferenceEngine::TensorDesc> inputDescs;
    std::vector<InferenceEngine::TensorDesc> outputDescs;
    for (size_t i = 0; i < getParentEdges().size(); i++) {
        inputs.push_back(getParentEdgeAt(i)->getBlob());
        if (isDynBatch && dynBatchLim >= inputs[inputs.size() - 1]->getTensorDesc().getDims()[0]) {
            isDynBatch = false;
        } else {
            // TODO: Ask the right dims using getShape() from previous node
            inputDescs.push_back(inputs[inputs.size() - 1]->getTensorDesc());
            inputDescs[inputDescs.size() - 1].getDims()[0] = static_cast<size_t>(batchToProcess());
        }
    }

    if (isDynBatch) {
        auto sts = extFactory->getShapes(inputDescs, outputDescs, nullptr);
        if (sts != InferenceEngine::StatusCode::OK)
            isDynBatch = false;
    }

    if (isDynBatch) {
        for (size_t i = 0; i < inputs.size(); i++) {
            auto td = inputs[i]->getTensorDesc();
            td.setDims(inputDescs[i].getDims());
            inputs[i] = make_blob_with_precision(td, getParentEdgeAt(i)->getMemory().GetData());
        }
    }
    std::vector<InferenceEngine::Blob::Ptr> outputs;
    for (size_t i = 0; i < getChildEdges().size(); i++) {
        if (isDynBatch) {
            size_t idx = i >= outputDescs.size() ? 0 : i;
            auto td = getChildEdgeAt(i)->getBlob()->getTensorDesc();
            td.setDims(outputDescs[idx].getDims());
            outputs.push_back(make_blob_with_precision(td, getChildEdgeAt(i)->getMemory().GetData()));
        } else {
            outputs.push_back(getChildEdgeAt(i)->getBlob());
        }
    }
    auto * execImpl = dynamic_cast<InferenceEngine::ILayerExecImpl *>(impls[0].get());
    if (execImpl != nullptr) {
        InferenceEngine::ResponseDesc resp;
        InferenceEngine::StatusCode rc = execImpl->execute(inputs, outputs, &resp);
        if (rc != InferenceEngine::OK) {
            THROW_IE_EXCEPTION << resp.msg;
        }
    }
}

MKLDNNGenericNode::~MKLDNNGenericNode() {
    extFactory.reset();
    extensionManager.reset();
}

void MKLDNNGenericNode::initDescriptor(const InferenceEngine::LayerConfig &config) {
    InferenceEngine::LayerConfig rightConfig = config;
    InferenceEngine::StatusCode rc;
    InferenceEngine::ResponseDesc resp;

    InferenceEngine::ILayerImpl::Ptr selectedImpl;
    for (size_t k = 0, t = 0; k < impls.size(); k++) {
        std::vector<InferenceEngine::LayerConfig> configs;
        rc = impls[k]->getSupportedConfigurations(configs, &resp);
        if (rc != InferenceEngine::OK) {
            THROW_IE_EXCEPTION << resp.msg;
        }
        for (size_t j = 0; j < configs.size(); j++, t++) {
            if (t == selectedPrimitiveDescriptorIndex) {
                selectedImpl = impls[k];
            }
        }
    }

    for (size_t j = 0; j < rightConfig.inConfs.size(); j++) {
        if (getParentEdgeAt(j)->getParent()->getChildEdges().size() > 1) {
            rightConfig.inConfs[j].inPlace = -1;
        }
    }
    for (auto &outConf : rightConfig.outConfs) {
        if (outConf.inPlace < getParentEdges().size() &&
            getParentEdgeAt(static_cast<size_t>(outConf.inPlace))->getParent()->getChildEdges().size() > 1) {
            outConf.inPlace = -1;
        }
    }


    impls.clear();
    impls.emplace_back(selectedImpl);
    rc = impls[0]->init(rightConfig, &resp);
    if (rc != InferenceEngine::OK) {
        THROW_IE_EXCEPTION << resp.msg;
    }

    auto descriptor = getSelectedPrimitiveDescriptor();
    if (descriptor != nullptr) {
        descriptor->getConfig() = rightConfig;
    }
    bool isConst = !rightConfig.inConfs.empty() || !rightConfig.outConfs.empty();
    for (const auto &inConf : rightConfig.inConfs) {
        isConst = isConst && inConf.constant;
    }
    for (const auto &outConf : rightConfig.outConfs) {
        isConst = isConst && outConf.constant;
    }
    if (isConst) {
        constant = ConstantType::Const;
    }
}

void MKLDNNGenericNode::initOptimalPrimitiveDescriptor() {
    auto descriptor = getSelectedPrimitiveDescriptor();
    if (descriptor != nullptr) {
        auto config = descriptor->getConfig();
        initDescriptor(config);
    }
}

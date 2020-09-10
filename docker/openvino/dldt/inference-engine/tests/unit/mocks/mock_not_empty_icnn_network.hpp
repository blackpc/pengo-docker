// Copyright (C) 2018 Intel Corporation
//
// SPDX-License-Identifier: Apache-2.0
//

#pragma once

#include "ie_plugin.hpp"
#include "ie_iexecutable_network.hpp"
#include <gmock/gmock.h>
#include <string>
#include <vector>

using namespace InferenceEngine;

class MockNotEmptyICNNNetwork : public ICNNNetwork {
public:
    static constexpr const char* INPUT_BLOB_NAME = "first_input";
    static constexpr const char* OUTPUT_BLOB_NAME = "first_output";
    MOCK_QUALIFIED_METHOD0(getPrecision, const noexcept, Precision ());
    void getOutputsInfo(OutputsDataMap& out) const noexcept override {
        out[OUTPUT_BLOB_NAME] = nullptr;
    };
    void getInputsInfo(InputsDataMap &inputs) const noexcept override {
        inputs[INPUT_BLOB_NAME] = nullptr;
    };
    MOCK_QUALIFIED_METHOD1(getInput, const noexcept, InputInfo::Ptr (const std::string &inputName));
    MOCK_QUALIFIED_METHOD2(getName, const noexcept, void (char* pName, size_t len));
    MOCK_QUALIFIED_METHOD0(layerCount, const noexcept, size_t ());
    MOCK_QUALIFIED_METHOD0(getName, const noexcept, const std::string& ());
    MOCK_QUALIFIED_METHOD1(getData, noexcept, DataPtr&(const char* dname));
    MOCK_QUALIFIED_METHOD1(addLayer, noexcept, void(const CNNLayerPtr& layer));
    MOCK_QUALIFIED_METHOD3(addOutput, noexcept, StatusCode (const std::string &, size_t , ResponseDesc*));
    MOCK_QUALIFIED_METHOD3(getLayerByName, const noexcept, StatusCode (const char* , CNNLayerPtr& , ResponseDesc* ));
    MOCK_QUALIFIED_METHOD1(setTargetDevice, noexcept, void (TargetDevice));
    MOCK_QUALIFIED_METHOD0(getTargetDevice, const noexcept, TargetDevice ());
    MOCK_QUALIFIED_METHOD1(setBatchSize, noexcept, StatusCode (const size_t size));
    MOCK_QUALIFIED_METHOD2(setBatchSize, noexcept, StatusCode (const size_t size, ResponseDesc*));
    MOCK_QUALIFIED_METHOD0(getBatchSize, const noexcept, size_t ());
    MOCK_QUALIFIED_METHOD0(getStats, const noexcept, InferenceEngine::ICNNNetworkStats& ());
    MOCK_QUALIFIED_METHOD0(Release, noexcept, void ());
    MOCK_QUALIFIED_METHOD1(getInputShapes, const noexcept, void (ICNNNetwork::InputShapes &));
    MOCK_QUALIFIED_METHOD2(reshape, noexcept, StatusCode (const ICNNNetwork::InputShapes &, ResponseDesc *));
    MOCK_QUALIFIED_METHOD2(AddExtension, noexcept, StatusCode (const IShapeInferExtensionPtr &, ResponseDesc *));
};

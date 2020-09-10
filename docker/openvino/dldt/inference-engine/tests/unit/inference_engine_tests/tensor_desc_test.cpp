// Copyright (C) 2018 Intel Corporation
//
// SPDX-License-Identifier: Apache-2.0
//

#include <ie_layouts.h>
#include <ie_blob.h>
#include <gtest/gtest.h>
#include <random>
#include <chrono>

#include "mock_allocator.hpp"

#include <cpp/ie_cnn_net_reader.h>
#include <gmock/gmock-spec-builders.h>

#ifdef WIN32
#define UNUSED
#else
#define UNUSED  __attribute__((unused))
#endif

using namespace ::testing;
using namespace std;
using namespace InferenceEngine;

class TensorDescTests: public ::testing::Test {
protected:
    virtual void TearDown() {
    }

    virtual void SetUp() {
    }

public:

};

TEST_F(TensorDescTests, CreateBlobWithIncorrectLayout) {
    ASSERT_THROW(make_shared_blob<float>(Precision::FP32, Layout::NC, {1, 3, 32}), details::InferenceEngineException);
}

TEST_F(TensorDescTests, CreateEmptyBlob) {
    Blob::Ptr blob = make_shared_blob<float>(Precision::FP32);

    ASSERT_EQ(blob->getTensorDesc().getLayout(), Layout::NCHW);
}

TEST_F(TensorDescTests, CreateBlockedBlob) {
    TensorDesc desc(Precision::FP32, {1, 4, 2, 1}, {{1, 2, 2, 1, 2}, {0, 1, 2, 3, 1}});
    float data[8] = {1, 2, 3, 4, 5, 6, 7, 8};
    Blob::Ptr blockedBlob = make_shared_blob<float>(desc, data);
    Blob::Ptr nchwBlob = make_shared_blob<float>({Precision::FP32, {1, 4, 2, 1}, Layout::NCHW}, data);
    ASSERT_NE(blockedBlob->getTensorDesc().offset(5), nchwBlob->getTensorDesc().offset(5));
    ASSERT_EQ(6, blockedBlob->getTensorDesc().offset(5));
    ASSERT_EQ(5, nchwBlob->getTensorDesc().offset(5));
    ASSERT_EQ(Layout::NCHW, nchwBlob->layout());
    ASSERT_EQ(Layout::BLOCKED, blockedBlob->layout());
}

TEST_F(TensorDescTests, CompareNHWCandNCHWLayouts) {
    TensorDesc descNCHW(Precision::FP32, {1, 3, 4, 2}, Layout::NCHW);
    TensorDesc descNHWC(Precision::FP32, {1, 3, 4, 2}, Layout::NHWC);
    SizeVector nchw = {0, 1, 2, 3};
    SizeVector nhwc = {0, 2, 3, 1};

    ASSERT_NE(descNCHW, descNHWC);
    ASSERT_NE(descNCHW.getBlockingDesc(), descNHWC.getBlockingDesc());
    ASSERT_NE(descNCHW.getBlockingDesc().getOrder(), descNHWC.getBlockingDesc().getOrder());
    ASSERT_EQ(descNCHW.getBlockingDesc().getOrder(), nchw);
    ASSERT_EQ(descNHWC.getBlockingDesc().getOrder(), nhwc);
}

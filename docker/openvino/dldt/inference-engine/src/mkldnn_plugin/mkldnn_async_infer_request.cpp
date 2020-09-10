// Copyright (C) 2018 Intel Corporation
//
// SPDX-License-Identifier: Apache-2.0
//

#include "mkldnn_async_infer_request.h"
#include <memory>

MKLDNNPlugin::MKLDNNAsyncInferRequest::MKLDNNAsyncInferRequest(const InferenceEngine::InferRequestInternal::Ptr &inferRequest,
                                                               const InferenceEngine::ITaskExecutor::Ptr &taskExecutor,
                                                               const InferenceEngine::TaskSynchronizer::Ptr &taskSynchronizer,
                                                               const InferenceEngine::ITaskExecutor::Ptr &callbackExecutor)
        : InferenceEngine::AsyncInferRequestThreadSafeDefault(inferRequest, taskExecutor, taskSynchronizer, callbackExecutor) {}

MKLDNNPlugin::MKLDNNAsyncInferRequest::~MKLDNNAsyncInferRequest() {
    waitAllAsyncTasks();
}

void MKLDNNPlugin::MKLDNNAsyncInferRequest::Infer() {
    _callbackManager.disableCallback();
    StartAsync();
    Wait(InferenceEngine::IInferRequest::WaitMode::RESULT_READY);
    _callbackManager.enableCallback();
}

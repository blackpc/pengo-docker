// Copyright (C) 2018 Intel Corporation
//
// SPDX-License-Identifier: Apache-2.0
//

#pragma once

#include <string>
#include <vector>

#include <gmock/gmock.h>

#include <cpp_interfaces/impl/ie_infer_async_request_thread_safe_internal.hpp>

using namespace InferenceEngine;

class MockAsyncInferRequestThreadSafeInternal : public AsyncInferRequestThreadSafeInternal {
public:
    typedef std::shared_ptr<MockAsyncInferRequestThreadSafeInternal> Ptr;

    void setRequestBusy() {
        AsyncInferRequestThreadSafeInternal::setIsRequestBusy(true);
    }

    bool isRequestBusy() {
        return AsyncInferRequestThreadSafeInternal::isRequestBusy();
    }

    MOCK_METHOD1(Wait, InferenceEngine::StatusCode(int64_t));

    MOCK_METHOD0(StartAsync_ThreadUnsafe, void());

    MOCK_METHOD1(GetUserData_ThreadUnsafe, void(void * *));

    MOCK_METHOD1(SetUserData_ThreadUnsafe, void(void * ));

    MOCK_METHOD0(Infer_ThreadUnsafe, void());

    MOCK_CONST_METHOD1(GetPerformanceCounts_ThreadUnsafe, void(std::map<std::string, InferenceEngineProfileInfo>
            &));

    MOCK_METHOD2(GetBlob_ThreadUnsafe, void(
            const char *name, Blob::Ptr
            &));

    MOCK_METHOD2(SetBlob_ThreadUnsafe, void(
            const char *name,
            const Blob::Ptr &));

    MOCK_METHOD1(SetCompletionCallback_ThreadUnsafe, void(IInferRequest::CompletionCallback));

	MOCK_METHOD1(SetBatch, void(int));
	MOCK_METHOD1(SetBatch_ThreadUnsafe, void(int));
};

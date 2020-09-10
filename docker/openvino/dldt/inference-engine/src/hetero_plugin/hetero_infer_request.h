// Copyright (C) 2018 Intel Corporation
//
// SPDX-License-Identifier: Apache-2.0
//

/**
 * @brief a header file for IInferRequest interface
 * @file ie_iinfer_request.hpp
 */

#pragma once

#include <map>
#include <string>
#include <vector>
#include <memory>
#include <unordered_set>
#include <ie_common.h>
#include <cpp_interfaces/impl/ie_infer_request_internal.hpp>
#include <cpp_interfaces/impl/ie_executable_network_internal.hpp>
#include <cpp/ie_infer_request.hpp>
#include <cpp/ie_executable_network.hpp>

namespace HeteroPlugin {

class HeteroInferRequest : public InferenceEngine::InferRequestInternal {
public:
    typedef std::shared_ptr<HeteroInferRequest> Ptr;

    struct SubRequestDesc {
        InferenceEngine::ExecutableNetwork::Ptr _network;
        InferenceEngine::InferRequest::Ptr _request;
        std::unordered_set<std::string> _iNames;
        std::unordered_set<std::string> _oNames;
        InferenceEngine::ProfilingTask _profilingTask;
    };
    using SubRequestsList = std::vector<SubRequestDesc>;

    explicit HeteroInferRequest(InferenceEngine::InputsDataMap networkInputs,
                                InferenceEngine::OutputsDataMap networkOutputs,
                                const SubRequestsList &inferRequests);

    void InferImpl() override;

    void
    GetPerformanceCounts(std::map<std::string, InferenceEngine::InferenceEngineProfileInfo> &perfMap) const override;

    void updateInOutIfNeeded();

    void setCallbackSequence();

    void startFirstAsyncRequest();

    InferenceEngine::StatusCode waitAllRequests(int64_t millis_timeout);

    void setCallbackForLastRequest(std::function<void(InferenceEngine::InferRequest, InferenceEngine::StatusCode)>& callback);

    bool isAnyRequestBusy();

private:
    SubRequestsList _inferRequests;
    std::map<std::string, InferenceEngine::Blob::Ptr> _blobs;
};

}  // namespace HeteroPlugin


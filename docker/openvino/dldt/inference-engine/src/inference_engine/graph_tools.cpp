// Copyright (C) 2018 Intel Corporation
//
// SPDX-License-Identifier: Apache-2.0
//

#include "graph_tools.hpp"
#include "details/ie_cnn_network_tools.h"
#include <vector>

using namespace std;

namespace InferenceEngine {
namespace details {

std::vector<CNNLayerPtr> CNNNetSortTopologically(const ICNNNetwork & network) {
    std::vector<CNNLayerPtr> stackOfVisited;
    bool res = CNNNetForestDFS(CNNNetGetAllInputLayers(network), [&](CNNLayerPtr  current){
        stackOfVisited.push_back(current);
    }, false);

    if (!res) {
        THROW_IE_EXCEPTION << "Sorting not possible, due to existed loop.";
    }

    std::reverse(std::begin(stackOfVisited), std::end(stackOfVisited));

    return stackOfVisited;
}

}   // namespace details
}  // namespace InferenceEngine
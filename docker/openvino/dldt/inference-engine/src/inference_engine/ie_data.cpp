// Copyright (C) 2018 Intel Corporation
//
// SPDX-License-Identifier: Apache-2.0
//

#include "ie_layers.h"
#include "ie_data.h"
#include "blob_factory.hpp"
#include <memory>
#include <string>
#include <map>

using namespace InferenceEngine;

Blob::Ptr Blob::CreateFromData(const DataPtr &data) {
    return CreateBlobFromData(data);
}

Data::Data(const std::string &name, Precision _precision, Layout layout): precision(_precision), layout(layout),
                                                                          name(name), userObject({0}),
                                                                          tensorDesc(_precision, layout) {}

Data::Data(const std::string &name, const SizeVector &a_dims, Precision _precision, Layout layout)
        : precision(_precision), layout(layout), dims(a_dims), name(name), userObject({0}),
          tensorDesc(_precision, a_dims, layout) {
    SizeVector tensorDims = a_dims;
    std::reverse(tensorDims.begin(), tensorDims.end());
    tensorDesc = TensorDesc(_precision, tensorDims, layout);
}

Data::Data(const std::string &name, const TensorDesc &desc): tensorDesc(desc), precision(desc.getPrecision()),
                                                             layout(desc.getLayout()), dims(desc.getDims()),
                                                             name(name), userObject({0}) {
    std::reverse(dims.begin(), dims.end());
}

const SizeVector& Data::getDims() const {
    return tensorDesc.getDims();
}

const Precision& Data::getPrecision() const {
    if (precision)
        return precision;

    return tensorDesc.getPrecision();
}

const TensorDesc& Data::getTensorDesc() const {
    if ((tensorDesc.getDims().size() == 0 && tensorDesc.getDims() != dims) ||
            (tensorDesc.getLayout() == Layout::ANY && layout != Layout::ANY) ||
            (!tensorDesc.getPrecision() && precision)) {
        THROW_IE_EXCEPTION << "Tensor descriptor is empty!";
    }
    return tensorDesc;
}

bool Data::isInitialized() const {
    return !dims.empty() || !tensorDesc.getDims().empty();
}

void Data::setDims(const SizeVector &a_dims) {
    dims = a_dims;
    std::reverse(dims.begin(), dims.end());
    tensorDesc.setDims(a_dims);
}

void Data::setBatchSize(size_t batch_size) {
    if (dims.empty()) {
        dims = tensorDesc.getDims();
        std::reverse(dims.begin(), dims.end());
    }
    if (dims.empty())
        return;
    dims.at(dims.size() - 1) = batch_size;
    SizeVector normalDims = dims;
    std::reverse(normalDims.begin(), normalDims.end());
    tensorDesc.setDims(normalDims);
}

void Data::setLayout(Layout layout) {
    tensorDesc.setLayout(layout);
    this->layout = layout;
}

CNNLayerWeakPtr &Data::getCreatorLayer() {
    return creatorLayer;
}

const std::string &Data::getName() const {
    return name;
}

std::map<std::string, CNNLayerPtr> &Data::getInputTo() {
    return inputTo;
}

const UserValue& Data::getUserObject() const {
    return userObject;
}

Layout Data::getLayout() const {
    if (tensorDesc.getLayout() == Layout::ANY && layout != Layout::ANY)
        return layout;
    return tensorDesc.getLayout();
}

void Data::setPrecision(const Precision & precision) {
    this->precision = precision;
    tensorDesc.setPrecision(precision);
}

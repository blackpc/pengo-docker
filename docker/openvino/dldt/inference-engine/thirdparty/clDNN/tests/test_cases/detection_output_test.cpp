/*
// Copyright (c) 2016 Intel Corporation
//
// Licensed under the Apache License, Version 2.0 (the "License");
// you may not use this file except in compliance with the License.
// You may obtain a copy of the License at
//
//      http://www.apache.org/licenses/LICENSE-2.0
//
// Unless required by applicable law or agreed to in writing, software
// distributed under the License is distributed on an "AS IS" BASIS,
// WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
// See the License for the specific language governing permissions and
// limitations under the License.
*/

///////////////////////////////////////////////////////////////////////////////////////////////////
#include <gtest/gtest.h>
#include "api/CPP/memory.hpp"
#include <api/CPP/input_layout.hpp>
#include "api/CPP/detection_output.hpp"
#include <api/CPP/topology.hpp>
#include <api/CPP/network.hpp>
#include <api/CPP/engine.hpp>
#include "test_utils/test_utils.h"

namespace cldnn
{
    template<> struct type_to_data_type<FLOAT16> { static const data_types value = data_types::f16; };
}

using namespace cldnn;
using namespace tests;

template <typename T>
class detection_output_test : public ::testing::Test
{

public:
    detection_output_test() :
        nms_threshold(0.1f)
    {}

    void init_buffers(cldnn::memory prior_memory, cldnn::memory confidence_memory, cldnn::memory location_memory,
                      bool share_location, bool variance_encoded_in_target = false,
                      int prior_info_size = 4, int prior_coordinates_offset = 0, bool prior_is_normalized = true)
    {
        auto location_ptr = location_memory.pointer<T>();
        auto confidence_ptr = confidence_memory.pointer<T>();
        auto prior_box_ptr = prior_memory.pointer<T>();

        T* prior_data = prior_box_ptr.data();
        T* confidence_data = confidence_ptr.data();
        T* location_data = location_ptr.data();

        // Fill prior-box data.
        const float step = 0.5f;
        const float box_size = 0.3f;
        const float prior_multiplier = prior_is_normalized ? 1.0f : static_cast<float>(this->img_size);
        const float variance = 0.1f;
        int idx = 0;
        for (int h = 0; h < 2; ++h)
        {
            float center_y = (h + 0.5f) * step;
            for (int w = 0; w < 2; ++w) 
            {
                float center_x = (w + 0.5f) * step;
                prior_data[idx+prior_coordinates_offset+0] = (center_x - box_size / 2) * prior_multiplier;
                prior_data[idx+prior_coordinates_offset+1] = (center_y - box_size / 2) * prior_multiplier;
                prior_data[idx+prior_coordinates_offset+2] = (center_x + box_size / 2) * prior_multiplier;
                prior_data[idx+prior_coordinates_offset+3] = (center_y + box_size / 2) * prior_multiplier;

                idx += prior_info_size;
            }
        }
        if (!variance_encoded_in_target)
        {
            for (int i = 0; i < idx; ++i)
            {
                prior_data[idx + i] = variance;
            }
        }

        // Fill confidences.
        idx = 0;
        for (int i = 0; i < num_of_images; ++i) 
        {
            for (int j = 0; j < num_priors; ++j) 
            {
                for (int c = 0; c < num_classes; ++c) 
                {
                    if (i % 2 == c % 2) 
                    {
                        confidence_data[idx++] = j * 0.2f;
                    }
                    else 
                    {
                        confidence_data[idx++] = 1 - j * 0.2f;
                    }
                }
            }
        }

        // Fill locations.
        const int num_loc_classes = share_location ? 1 : num_classes;
        const float loc_multiplier = variance_encoded_in_target ? variance : 1.0f;
        idx = 0;
        for (int i = 0; i < num_of_images; ++i) 
        {
            for (int h = 0; h < 2; ++h) 
            {
                for (int w = 0; w < 2; ++w) 
                {
                    for (int c = 0; c < num_loc_classes; ++c) 
                    {
                        location_data[idx++] = (w % 2 ? -1 : 1) * (i * 1 + c / 2.f + 0.5f) * loc_multiplier;
                        location_data[idx++] = (h % 2 ? -1 : 1) * (i * 1 + c / 2.f + 0.5f) * loc_multiplier;
                        location_data[idx++] = (w % 2 ? -1 : 1) * (i * 1 + c / 2.f + 0.5f) * loc_multiplier;
                        location_data[idx++] = (h % 2 ? -1 : 1) * (i * 1 + c / 2.f + 0.5f) * loc_multiplier;
                    }
                }
            }
        }
    }

    void check_results(const memory& output, const int num, const std::string values)
    {
        assert(num < output.get_layout().size.spatial[1]);

        // Split values to vector of items.
        std::vector<std::string> items;
        std::istringstream iss(values);
        std::copy(std::istream_iterator<std::string>(iss), std::istream_iterator<std::string>(), back_inserter(items));
        EXPECT_EQ((int)items.size(), 7);

        // Check data.
        auto out_ptr = output.pointer<T>();
        const T* data = out_ptr.data();
        for (int i = 0; i < 2; ++i)
        {
            EXPECT_EQ(static_cast<int>((float)data[num * output.get_layout().size.spatial[0] + i]), atoi(items[i].c_str()));
        }
        for (int i = 2; i < 7; ++i) 
        {
            EXPECT_TRUE(floating_point_equal(data[num * output.get_layout().size.spatial[0] + i], (T)(float)atof(items[i].c_str())));
        }
    }
    static const int num_of_images = 2;
    static const int num_classes = 2;
    static const int num_priors = 4;
    static const int img_size = 300;
    const float nms_threshold;
};

typedef ::testing::Types<float, FLOAT16> detection_output_test_types;
TYPED_TEST_CASE(detection_output_test, detection_output_test_types);


TYPED_TEST(detection_output_test, test_setup_basic)
{
    const bool share_location = true;
    const int num_loc_classes = share_location ? 1 : this->num_classes;
    const int keep_top_k = 150;

    cldnn::engine engine;
    cldnn::memory input_location = memory::allocate(engine, { type_to_data_type<TypeParam>::value, format::bfyx,{ this->num_of_images, this->num_priors * num_loc_classes * 4, 1, 1 } });
    cldnn::memory input_confidence = memory::allocate(engine, { type_to_data_type<TypeParam>::value, format::bfyx,{ this->num_of_images, this->num_priors * this->num_classes, 1, 1 } });
    cldnn::memory input_prior_box = memory::allocate(engine, { type_to_data_type<TypeParam>::value, format::bfyx,{ 1, 2, 1, this->num_priors * 4} });

    topology topology;
    topology.add(input_layout("input_location", input_location.get_layout()));
    topology.add(input_layout("input_confidence", input_confidence.get_layout()));
    topology.add(input_layout("input_prior_box", input_prior_box.get_layout()));

    topology.add(detection_output("detection_output", "input_location", "input_confidence", "input_prior_box", this->num_classes, keep_top_k));
    network network(engine, topology);
    network.set_input_data("input_location", input_location);
    network.set_input_data("input_confidence", input_confidence);
    network.set_input_data("input_prior_box", input_prior_box);

    auto outputs = network.execute();
    
    EXPECT_EQ(outputs.size(), size_t(1));
    EXPECT_EQ(outputs.begin()->first, "detection_output");
    
    EXPECT_EQ(outputs.begin()->second.get_memory().get_layout().size.batch[0], 1);
    EXPECT_EQ(outputs.begin()->second.get_memory().get_layout().size.feature[0], 1);
    EXPECT_EQ(outputs.begin()->second.get_memory().get_layout().size.spatial[1], keep_top_k * this->num_of_images);
    EXPECT_EQ(outputs.begin()->second.get_memory().get_layout().size.spatial[0], 7);
}

TYPED_TEST(detection_output_test, test_forward_share_location)
{
    const bool share_location = true;
    const int num_loc_classes = share_location ? 1 : this->num_classes;
    const int keep_top_k = 4;
    const int background_label_id = 0;

    cldnn::engine engine;
    cldnn::memory input_location = memory::allocate(engine, { type_to_data_type<TypeParam>::value, format::bfyx,{ this->num_of_images, this->num_priors * num_loc_classes * 4, 1, 1 } });
    cldnn::memory input_confidence = memory::allocate(engine, { type_to_data_type<TypeParam>::value, format::bfyx,{ this->num_of_images, this->num_priors * this->num_classes, 1, 1 } });
    cldnn::memory input_prior_box = memory::allocate(engine, { type_to_data_type<TypeParam>::value, format::bfyx,{ 1, 2, 1, this->num_priors * 4} });

    this->init_buffers(input_prior_box, input_confidence, input_location, share_location);

    topology topology;
    topology.add(input_layout("input_location", input_location.get_layout()));
    topology.add(input_layout("input_confidence", input_confidence.get_layout()));
    topology.add(input_layout("input_prior_box", input_prior_box.get_layout()));

    topology.add(detection_output("detection_output", "input_location", "input_confidence", "input_prior_box", this->num_classes, keep_top_k, share_location, background_label_id, this->nms_threshold));
    network network(engine, topology);
    network.set_input_data("input_location", input_location);
    network.set_input_data("input_confidence", input_confidence);
    network.set_input_data("input_prior_box", input_prior_box);

    auto outputs = network.execute();

    EXPECT_EQ(outputs.size(), size_t(1));
    EXPECT_EQ(outputs.begin()->first, "detection_output");

    EXPECT_EQ(outputs.begin()->second.get_memory().get_layout().size.batch[0], 1);
    EXPECT_EQ(outputs.begin()->second.get_memory().get_layout().size.feature[0], 1);
    EXPECT_EQ(outputs.begin()->second.get_memory().get_layout().size.spatial[1], keep_top_k * this->num_of_images);
    EXPECT_EQ(outputs.begin()->second.get_memory().get_layout().size.spatial[0], 7);

    auto output_prim = outputs.begin()->second.get_memory();
    
    this->check_results(output_prim, 0, "0 1 1.0 0.15 0.15 0.45 0.45");
    this->check_results(output_prim, 1, "0 1 0.8 0.55 0.15 0.85 0.45");
    this->check_results(output_prim, 2, "0 1 0.6 0.15 0.55 0.45 0.85");
    this->check_results(output_prim, 3, "0 1 0.4 0.55 0.55 0.85 0.85");
    this->check_results(output_prim, 4, "1 1 0.6 0.45 0.45 0.75 0.75");
    this->check_results(output_prim, 5, "1 1 0.0 0.25 0.25 0.55 0.55");
    this->check_results(output_prim, 6, "-1 0 0 0 0 0 0");
    this->check_results(output_prim, 7, "-1 0 0 0 0 0 0");
}

TYPED_TEST(detection_output_test, test_forward_num_detections_greater_than_keep_top_k)
{
    const bool share_location = true;
    const int num_loc_classes = share_location ? 1 : this->num_classes;
    const int keep_top_k = 1;
    const int background_label_id = 0;

    cldnn::engine engine;
    cldnn::memory input_location = memory::allocate(engine, { type_to_data_type<TypeParam>::value, format::bfyx,{ this->num_of_images, this->num_priors * num_loc_classes * 4, 1, 1 } });
    cldnn::memory input_confidence = memory::allocate(engine, { type_to_data_type<TypeParam>::value, format::bfyx,{ this->num_of_images, this->num_priors * this->num_classes, 1, 1 } });
    cldnn::memory input_prior_box = memory::allocate(engine, { type_to_data_type<TypeParam>::value, format::bfyx,{ 1, 2, 1, this->num_priors * 4} });

    this->init_buffers(input_prior_box, input_confidence, input_location, share_location);

    topology topology;
    topology.add(input_layout("input_location", input_location.get_layout()));
    topology.add(input_layout("input_confidence", input_confidence.get_layout()));
    topology.add(input_layout("input_prior_box", input_prior_box.get_layout()));

    topology.add(detection_output("detection_output", "input_location", "input_confidence", "input_prior_box", this->num_classes, keep_top_k, share_location, background_label_id, this->nms_threshold));
    network network(engine, topology);
    network.set_input_data("input_location", input_location);
    network.set_input_data("input_confidence", input_confidence);
    network.set_input_data("input_prior_box", input_prior_box);

    auto outputs = network.execute();

    EXPECT_EQ(outputs.size(), size_t(1));
    EXPECT_EQ(outputs.begin()->first, "detection_output");

    EXPECT_EQ(outputs.begin()->second.get_memory().get_layout().size.batch[0], 1);
    EXPECT_EQ(outputs.begin()->second.get_memory().get_layout().size.feature[0], 1);
    EXPECT_EQ(outputs.begin()->second.get_memory().get_layout().size.spatial[1], keep_top_k * this->num_of_images);
    EXPECT_EQ(outputs.begin()->second.get_memory().get_layout().size.spatial[0], 7);

    auto output_prim = outputs.begin()->second.get_memory();

    this->check_results(output_prim, 0, "0 1 1.0 0.15 0.15 0.45 0.45");
    this->check_results(output_prim, 1, "1 1 0.6 0.45 0.45 0.75 0.75");
}

TYPED_TEST(detection_output_test, test_forward_num_detections_smaller_than_keep_top_k)
{
    const bool share_location = true;
    const int num_loc_classes = share_location ? 1 : this->num_classes;
    const int keep_top_k = 6;
    const int background_label_id = 0;

    cldnn::engine engine;
    cldnn::memory input_location = memory::allocate(engine, { type_to_data_type<TypeParam>::value, format::bfyx,{ this->num_of_images, this->num_priors * num_loc_classes * 4, 1, 1 } });
    cldnn::memory input_confidence = memory::allocate(engine, { type_to_data_type<TypeParam>::value, format::bfyx,{ this->num_of_images, this->num_priors * this->num_classes, 1, 1 } });
    cldnn::memory input_prior_box = memory::allocate(engine, { type_to_data_type<TypeParam>::value, format::bfyx,{ 1, 2, 1, this->num_priors * 4} });

    this->init_buffers(input_prior_box, input_confidence, input_location, share_location);

    topology topology;
    topology.add(input_layout("input_location", input_location.get_layout()));
    topology.add(input_layout("input_confidence", input_confidence.get_layout()));
    topology.add(input_layout("input_prior_box", input_prior_box.get_layout()));

    topology.add(detection_output("detection_output", "input_location", "input_confidence", "input_prior_box", this->num_classes, keep_top_k, share_location, background_label_id, this->nms_threshold));
    network network(engine, topology);
    network.set_input_data("input_location", input_location);
    network.set_input_data("input_confidence", input_confidence);
    network.set_input_data("input_prior_box", input_prior_box);

    auto outputs = network.execute();

    EXPECT_EQ(outputs.size(), size_t(1));
    EXPECT_EQ(outputs.begin()->first, "detection_output");

    EXPECT_EQ(outputs.begin()->second.get_memory().get_layout().size.batch[0], 1);
    EXPECT_EQ(outputs.begin()->second.get_memory().get_layout().size.feature[0], 1);
    EXPECT_EQ(outputs.begin()->second.get_memory().get_layout().size.spatial[1], keep_top_k * this->num_of_images);
    EXPECT_EQ(outputs.begin()->second.get_memory().get_layout().size.spatial[0], 7);

    auto output_prim = outputs.begin()->second.get_memory();

    this->check_results(output_prim, 0, "0 1 1.0 0.15 0.15 0.45 0.45");
    this->check_results(output_prim, 1, "0 1 0.8 0.55 0.15 0.85 0.45");
    this->check_results(output_prim, 2, "0 1 0.6 0.15 0.55 0.45 0.85");
    this->check_results(output_prim, 3, "0 1 0.4 0.55 0.55 0.85 0.85");
    this->check_results(output_prim, 4, "1 1 0.6 0.45 0.45 0.75 0.75");
    this->check_results(output_prim, 5, "1 1 0.0 0.25 0.25 0.55 0.55");
    this->check_results(output_prim, 6, "-1 0 0 0 0 0 0");
    this->check_results(output_prim, 7, "-1 0 0 0 0 0 0");
    this->check_results(output_prim, 8, "-1 0 0 0 0 0 0");
    this->check_results(output_prim, 9, "-1 0 0 0 0 0 0");
    this->check_results(output_prim, 10, "-1 0 0 0 0 0 0");
    this->check_results(output_prim, 11, "-1 0 0 0 0 0 0");
}

TYPED_TEST(detection_output_test, test_forward_share_location_top_k)
{
    const bool share_location = true;
    const int num_loc_classes = share_location ? 1 : this->num_classes;
    const int keep_top_k = 2;
    const int top_k = 2;
    const int background_label_id = 0;

    cldnn::engine engine;
    cldnn::memory input_location = memory::allocate(engine, { type_to_data_type<TypeParam>::value, format::bfyx,{ this->num_of_images, this->num_priors * num_loc_classes * 4, 1, 1 } });
    cldnn::memory input_confidence = memory::allocate(engine, { type_to_data_type<TypeParam>::value, format::bfyx,{ this->num_of_images, this->num_priors * this->num_classes, 1, 1 } });
    cldnn::memory input_prior_box = memory::allocate(engine, { type_to_data_type<TypeParam>::value, format::bfyx,{ 1, 2, 1, this->num_priors * 4 } });

    this->init_buffers(input_prior_box, input_confidence, input_location, share_location);

    topology topology;
    topology.add(input_layout("input_location", input_location.get_layout()));
    topology.add(input_layout("input_confidence", input_confidence.get_layout()));
    topology.add(input_layout("input_prior_box", input_prior_box.get_layout()));

    topology.add(detection_output("detection_output", "input_location", "input_confidence", "input_prior_box", this->num_classes, keep_top_k, share_location, background_label_id, this->nms_threshold, top_k));
    network network(engine, topology);
    network.set_input_data("input_location", input_location);
    network.set_input_data("input_confidence", input_confidence);
    network.set_input_data("input_prior_box", input_prior_box);

    auto outputs = network.execute();

    EXPECT_EQ(outputs.size(), size_t(1));
    EXPECT_EQ(outputs.begin()->first, "detection_output");

    EXPECT_EQ(outputs.begin()->second.get_memory().get_layout().size.batch[0], 1);
    EXPECT_EQ(outputs.begin()->second.get_memory().get_layout().size.feature[0], 1);
    EXPECT_EQ(outputs.begin()->second.get_memory().get_layout().size.spatial[1], keep_top_k * this->num_of_images);
    EXPECT_EQ(outputs.begin()->second.get_memory().get_layout().size.spatial[0], 7);

    auto output_prim = outputs.begin()->second.get_memory();

    this->check_results(output_prim, 0, "0 1 1.0 0.15 0.15 0.45 0.45");
    this->check_results(output_prim, 1, "0 1 0.8 0.55 0.15 0.85 0.45");
    this->check_results(output_prim, 2, "1 1 0.6 0.45 0.45 0.75 0.75");
    this->check_results(output_prim, 3, "-1 0 0 0 0 0 0");
}

TYPED_TEST(detection_output_test, test_forward_no_share_location)
{
    const bool share_location = false;
    const int num_loc_classes = share_location ? 1 : this->num_classes;
    const int keep_top_k = 10;
    const int background_label_id = -1;

    cldnn::engine engine;
    cldnn::memory input_location = memory::allocate(engine, { type_to_data_type<TypeParam>::value, format::bfyx,{ this->num_of_images, this->num_priors * num_loc_classes * 4, 1, 1 } });
    cldnn::memory input_confidence = memory::allocate(engine, { type_to_data_type<TypeParam>::value, format::bfyx,{ this->num_of_images, this->num_priors * this->num_classes, 1, 1 } });
    cldnn::memory input_prior_box = memory::allocate(engine, { type_to_data_type<TypeParam>::value, format::bfyx,{ 1, 2, 1, this->num_priors * 4 } });

    this->init_buffers(input_prior_box, input_confidence, input_location, share_location);

    topology topology;
    topology.add(input_layout("input_location", input_location.get_layout()));
    topology.add(input_layout("input_confidence", input_confidence.get_layout()));
    topology.add(input_layout("input_prior_box", input_prior_box.get_layout()));

    topology.add(detection_output("detection_output", "input_location", "input_confidence", "input_prior_box", this->num_classes, keep_top_k, share_location, background_label_id, this->nms_threshold));
    network network(engine, topology);
    network.set_input_data("input_location", input_location);
    network.set_input_data("input_confidence", input_confidence);
    network.set_input_data("input_prior_box", input_prior_box);

    auto outputs = network.execute();

    EXPECT_EQ(outputs.size(), size_t(1));
    EXPECT_EQ(outputs.begin()->first, "detection_output");

    EXPECT_EQ(outputs.begin()->second.get_memory().get_layout().size.batch[0], 1);
    EXPECT_EQ(outputs.begin()->second.get_memory().get_layout().size.feature[0], 1);
    EXPECT_EQ(outputs.begin()->second.get_memory().get_layout().size.spatial[1], keep_top_k * this->num_of_images);
    EXPECT_EQ(outputs.begin()->second.get_memory().get_layout().size.spatial[0], 7);

    auto output_prim = outputs.begin()->second.get_memory();

    this->check_results(output_prim, 0, "0 0 0.6 0.55 0.55 0.85 0.85");
    this->check_results(output_prim, 1, "0 0 0.4 0.15 0.55 0.45 0.85");
    this->check_results(output_prim, 2, "0 0 0.2 0.55 0.15 0.85 0.45");
    this->check_results(output_prim, 3, "0 0 0.0 0.15 0.15 0.45 0.45");
    this->check_results(output_prim, 4, "0 1 1.0 0.20 0.20 0.50 0.50");
    this->check_results(output_prim, 5, "0 1 0.8 0.50 0.20 0.80 0.50");
    this->check_results(output_prim, 6, "0 1 0.6 0.20 0.50 0.50 0.80");
    this->check_results(output_prim, 7, "0 1 0.4 0.50 0.50 0.80 0.80");
    this->check_results(output_prim, 8, "1 0 1.0 0.25 0.25 0.55 0.55");
    this->check_results(output_prim, 9, "1 0 0.4 0.45 0.45 0.75 0.75");
    this->check_results(output_prim, 10, "1 1 0.6 0.40 0.40 0.70 0.70");
    this->check_results(output_prim, 11, "-1 0 0 0 0 0 0");
    this->check_results(output_prim, 12, "-1 0 0 0 0 0 0");
    this->check_results(output_prim, 13, "-1 0 0 0 0 0 0");
    this->check_results(output_prim, 14, "-1 0 0 0 0 0 0");
    this->check_results(output_prim, 15, "-1 0 0 0 0 0 0");
    this->check_results(output_prim, 16, "-1 0 0 0 0 0 0");
    this->check_results(output_prim, 17, "-1 0 0 0 0 0 0");
    this->check_results(output_prim, 18, "-1 0 0 0 0 0 0");
    this->check_results(output_prim, 19, "-1 0 0 0 0 0 0");
}

TYPED_TEST(detection_output_test, test_forward_no_share_location_top_k)
{
    const bool share_location = false;
    const int num_loc_classes = share_location ? 1 : this->num_classes;
    const int keep_top_k = 4;
    const int background_label_id = -1;
    const int top_k = 2;

    cldnn::engine engine;
    cldnn::memory input_location = memory::allocate(engine, { type_to_data_type<TypeParam>::value, format::bfyx,{ this->num_of_images, this->num_priors * num_loc_classes * 4, 1, 1 } });
    cldnn::memory input_confidence = memory::allocate(engine, { type_to_data_type<TypeParam>::value, format::bfyx,{ this->num_of_images, this->num_priors * this->num_classes, 1, 1 } });
    cldnn::memory input_prior_box = memory::allocate(engine, { type_to_data_type<TypeParam>::value, format::bfyx,{ 1, 2, 1, this->num_priors * 4 } });

    this->init_buffers(input_prior_box, input_confidence, input_location, share_location);

    topology topology;
    topology.add(input_layout("input_location", input_location.get_layout()));
    topology.add(input_layout("input_confidence", input_confidence.get_layout()));
    topology.add(input_layout("input_prior_box", input_prior_box.get_layout()));

    topology.add(detection_output("detection_output", "input_location", "input_confidence", "input_prior_box", this->num_classes, keep_top_k, share_location, background_label_id, this->nms_threshold, top_k));
    network network(engine, topology);
    network.set_input_data("input_location", input_location);
    network.set_input_data("input_confidence", input_confidence);
    network.set_input_data("input_prior_box", input_prior_box);

    auto outputs = network.execute();

    EXPECT_EQ(outputs.size(), size_t(1));
    EXPECT_EQ(outputs.begin()->first, "detection_output");

    EXPECT_EQ(outputs.begin()->second.get_memory().get_layout().size.batch[0], 1);
    EXPECT_EQ(outputs.begin()->second.get_memory().get_layout().size.feature[0], 1);
    EXPECT_EQ(outputs.begin()->second.get_memory().get_layout().size.spatial[1], keep_top_k * this->num_of_images);
    EXPECT_EQ(outputs.begin()->second.get_memory().get_layout().size.spatial[0], 7);

    auto output_prim = outputs.begin()->second.get_memory();

    this->check_results(output_prim, 0, "0 0 0.6 0.55 0.55 0.85 0.85");
    this->check_results(output_prim, 1, "0 0 0.4 0.15 0.55 0.45 0.85");
    this->check_results(output_prim, 2, "0 1 1.0 0.20 0.20 0.50 0.50");
    this->check_results(output_prim, 3, "0 1 0.8 0.50 0.20 0.80 0.50");
    this->check_results(output_prim, 4, "1 0 1.0 0.25 0.25 0.55 0.55");
    this->check_results(output_prim, 5, "1 1 0.6 0.40 0.40 0.70 0.70");
    this->check_results(output_prim, 6, "-1 0 0 0 0 0 0");
    this->check_results(output_prim, 7, "-1 0 0 0 0 0 0");
}

TYPED_TEST(detection_output_test, test_forward_no_share_location_neg_0)
{
    const bool share_location = false;
    const int num_loc_classes = share_location ? 1 : this->num_classes;
    const int keep_top_k = 5;
    const int background_label_id = 0;

    cldnn::engine engine;
    cldnn::memory input_location = memory::allocate(engine, { type_to_data_type<TypeParam>::value, format::bfyx,{ this->num_of_images, this->num_priors * num_loc_classes * 4, 1, 1 } });
    cldnn::memory input_confidence = memory::allocate(engine, { type_to_data_type<TypeParam>::value, format::bfyx,{ this->num_of_images, this->num_priors * this->num_classes, 1, 1 } });
    cldnn::memory input_prior_box = memory::allocate(engine, { type_to_data_type<TypeParam>::value, format::bfyx,{ 1, 2, 1, this->num_priors * 4 } });

    this->init_buffers(input_prior_box, input_confidence, input_location, share_location);

    topology topology;
    topology.add(input_layout("input_location", input_location.get_layout()));
    topology.add(input_layout("input_confidence", input_confidence.get_layout()));
    topology.add(input_layout("input_prior_box", input_prior_box.get_layout()));

    topology.add(detection_output("detection_output", "input_location", "input_confidence", "input_prior_box", this->num_classes, keep_top_k, share_location, background_label_id, this->nms_threshold));
    network network(engine, topology);
    network.set_input_data("input_location", input_location);
    network.set_input_data("input_confidence", input_confidence);
    network.set_input_data("input_prior_box", input_prior_box);

    auto outputs = network.execute();

    EXPECT_EQ(outputs.size(), size_t(1));
    EXPECT_EQ(outputs.begin()->first, "detection_output");

    EXPECT_EQ(outputs.begin()->second.get_memory().get_layout().size.batch[0], 1);
    EXPECT_EQ(outputs.begin()->second.get_memory().get_layout().size.feature[0], 1);
    EXPECT_EQ(outputs.begin()->second.get_memory().get_layout().size.spatial[1], keep_top_k * this->num_of_images);
    EXPECT_EQ(outputs.begin()->second.get_memory().get_layout().size.spatial[0], 7);

    auto output_prim = outputs.begin()->second.get_memory();

    this->check_results(output_prim, 0, "0 1 1.0 0.20 0.20 0.50 0.50");
    this->check_results(output_prim, 1, "0 1 0.8 0.50 0.20 0.80 0.50");
    this->check_results(output_prim, 2, "0 1 0.6 0.20 0.50 0.50 0.80");
    this->check_results(output_prim, 3, "0 1 0.4 0.50 0.50 0.80 0.80");
    this->check_results(output_prim, 4, "1 1 0.6 0.40 0.40 0.70 0.70");
    this->check_results(output_prim, 5, "-1 0 0 0 0 0 0");
    this->check_results(output_prim, 6, "-1 0 0 0 0 0 0");
    this->check_results(output_prim, 7, "-1 0 0 0 0 0 0");
    this->check_results(output_prim, 8, "-1 0 0 0 0 0 0");
    this->check_results(output_prim, 9, "-1 0 0 0 0 0 0");
}

TYPED_TEST(detection_output_test, test_forward_no_share_location_neg_0_top_k)
{
    const bool share_location = false;
    const int num_loc_classes = share_location ? 1 : this->num_classes;
    const int keep_top_k = 2;
    const int background_label_id = 0;
    const int top_k = 2;

    cldnn::engine engine;
    cldnn::memory input_location = memory::allocate(engine, { type_to_data_type<TypeParam>::value, format::bfyx, { this->num_of_images, this->num_priors * num_loc_classes * 4, 1, 1 } });
    cldnn::memory input_confidence = memory::allocate(engine, { type_to_data_type<TypeParam>::value, format::bfyx, { this->num_of_images, this->num_priors * this->num_classes, 1, 1 } });
    cldnn::memory input_prior_box = memory::allocate(engine, { type_to_data_type<TypeParam>::value, format::bfyx, { 1, 2, 1, this->num_priors * 4 } });

    this->init_buffers(input_prior_box, input_confidence, input_location, share_location);

    topology topology;
    topology.add(input_layout("input_location", input_location.get_layout()));
    topology.add(input_layout("input_confidence", input_confidence.get_layout()));
    topology.add(input_layout("input_prior_box", input_prior_box.get_layout()));

    topology.add(detection_output("detection_output", "input_location", "input_confidence", "input_prior_box", this->num_classes, keep_top_k, share_location, background_label_id, this->nms_threshold, top_k));
    network network(engine, topology);
    network.set_input_data("input_location", input_location);
    network.set_input_data("input_confidence", input_confidence);
    network.set_input_data("input_prior_box", input_prior_box);

    auto outputs = network.execute();

    EXPECT_EQ(outputs.size(), size_t(1));
    EXPECT_EQ(outputs.begin()->first, "detection_output");

    EXPECT_EQ(outputs.begin()->second.get_memory().get_layout().size.batch[0], 1);
    EXPECT_EQ(outputs.begin()->second.get_memory().get_layout().size.feature[0], 1);
    EXPECT_EQ(outputs.begin()->second.get_memory().get_layout().size.spatial[1], keep_top_k * this->num_of_images);
    EXPECT_EQ(outputs.begin()->second.get_memory().get_layout().size.spatial[0], 7);

    auto output_prim = outputs.begin()->second.get_memory();

    this->check_results(output_prim, 0, "0 1 1.0 0.20 0.20 0.50 0.50");
    this->check_results(output_prim, 1, "0 1 0.8 0.50 0.20 0.80 0.50");
    this->check_results(output_prim, 2, "1 1 0.6 0.40 0.40 0.70 0.70");
    this->check_results(output_prim, 3, "-1 0 0 0 0 0 0");
}

TYPED_TEST(detection_output_test, test_forward_no_share_location_top_k_input_padding)
{
    const bool share_location = false;
    const int num_loc_classes = share_location ? 1 : this->num_classes;
    const int keep_top_k = 4;
    const int background_label_id = -1;
    const int top_k = 2;

    cldnn::engine engine;
    cldnn::memory input_location = memory::allocate(engine, { type_to_data_type<TypeParam>::value, format::bfyx,{ this->num_of_images, this->num_priors * num_loc_classes * 4, 1, 1 } });
    cldnn::memory input_confidence = memory::allocate(engine, { type_to_data_type<TypeParam>::value, format::bfyx,{ this->num_of_images, this->num_priors * this->num_classes, 1, 1 } });
    cldnn::memory input_prior_box = memory::allocate(engine, { type_to_data_type<TypeParam>::value, format::bfyx,{ 1, 2, 1, this->num_priors * 4 } });

    this->init_buffers(input_prior_box, input_confidence, input_location, share_location);
    topology topology;
    topology.add(input_layout("input_location", input_location.get_layout()));
    topology.add(input_layout("input_confidence", input_confidence.get_layout()));
    topology.add(input_layout("input_prior_box", input_prior_box.get_layout()));
    topology.add(reorder("input_location_padded", "input_location", input_location.get_layout().with_padding({ { 0, 0, 12, 3 },{ 0, 0, 5, 11 } })));
    topology.add(reorder("input_confidence_padded", "input_confidence", input_location.get_layout().with_padding({ { 0, 0, 2, 7 },{ 0, 0, 13, 1 } })));

    topology.add(detection_output("detection_output", "input_location_padded", "input_confidence_padded", "input_prior_box", this->num_classes, keep_top_k, share_location, background_label_id, this->nms_threshold, top_k));
    network network(engine, topology);
    network.set_input_data("input_location", input_location);
    network.set_input_data("input_confidence", input_confidence);
    network.set_input_data("input_prior_box", input_prior_box);

    auto outputs = network.execute();

    EXPECT_EQ(outputs.size(), size_t(1));
    EXPECT_EQ(outputs.begin()->first, "detection_output");

    EXPECT_EQ(outputs.begin()->second.get_memory().get_layout().size.batch[0], 1);
    EXPECT_EQ(outputs.begin()->second.get_memory().get_layout().size.feature[0], 1);
    EXPECT_EQ(outputs.begin()->second.get_memory().get_layout().size.spatial[1], keep_top_k * this->num_of_images);
    EXPECT_EQ(outputs.begin()->second.get_memory().get_layout().size.spatial[0], 7);

    auto output_prim = outputs.begin()->second.get_memory();

    this->check_results(output_prim, 0, "0 0 0.6 0.55 0.55 0.85 0.85");
    this->check_results(output_prim, 1, "0 0 0.4 0.15 0.55 0.45 0.85");
    this->check_results(output_prim, 2, "0 1 1.0 0.20 0.20 0.50 0.50");
    this->check_results(output_prim, 3, "0 1 0.8 0.50 0.20 0.80 0.50");
    this->check_results(output_prim, 4, "1 0 1.0 0.25 0.25 0.55 0.55");
    this->check_results(output_prim, 5, "1 1 0.6 0.40 0.40 0.70 0.70");
    this->check_results(output_prim, 6, "-1 0 0 0 0 0 0");
    this->check_results(output_prim, 7, "-1 0 0 0 0 0 0");
}

TYPED_TEST(detection_output_test, test_forward_no_share_location_top_k_faster_rcnn_case)
{
    const bool share_location = false;
    const int num_loc_classes = share_location ? 1 : this->num_classes;
    const int keep_top_k = 4;
    const int background_label_id = -1;
    const int top_k = 2;
    const float eta = 1.0f;
    const prior_box_code_type code_type = prior_box_code_type::corner;
    const bool variance_encoded_in_target = true;
    const float confidence_threshold = -std::numeric_limits<float>::max();
    const int32_t prior_info_size = 5;
    const int32_t prior_coordinates_offset = 1;
    const bool prior_is_normalized = true;

    cldnn::engine engine;
    cldnn::memory input_location = memory::allocate(engine, { type_to_data_type<TypeParam>::value, format::bfyx,{ this->num_of_images, this->num_priors * num_loc_classes * 4, 1, 1 } });
    cldnn::memory input_confidence = memory::allocate(engine, { type_to_data_type<TypeParam>::value, format::bfyx,{ this->num_of_images, this->num_priors * this->num_classes, 1, 1 } });
    cldnn::memory input_prior_box = memory::allocate(engine, { type_to_data_type<TypeParam>::value, format::bfyx,{ 1, 1, 1, this->num_priors * prior_info_size } });

    this->init_buffers(input_prior_box, input_confidence, input_location, share_location, variance_encoded_in_target,
                       prior_info_size, prior_coordinates_offset, prior_is_normalized);

    topology topology;
    topology.add(input_layout("input_location", input_location.get_layout()));
    topology.add(input_layout("input_confidence", input_confidence.get_layout()));
    topology.add(input_layout("input_prior_box", input_prior_box.get_layout()));
    topology.add(reorder("input_location_padded", "input_location", input_location.get_layout().with_padding({ { 0, 0, 12, 3 },{ 0, 0, 5, 11 } })));
    topology.add(reorder("input_confidence_padded", "input_confidence", input_location.get_layout().with_padding({ { 0, 0, 2, 7 },{ 0, 0, 13, 1 } })));

    topology.add(detection_output("detection_output", "input_location_padded", "input_confidence_padded", "input_prior_box",
                                  this->num_classes, keep_top_k, share_location, background_label_id, this->nms_threshold, top_k,
                                  eta, code_type, variance_encoded_in_target, confidence_threshold, prior_info_size, prior_coordinates_offset,
                                  prior_is_normalized, this->img_size, this->img_size
                    ));
    network network(engine, topology);
    network.set_input_data("input_location", input_location);
    network.set_input_data("input_confidence", input_confidence);
    network.set_input_data("input_prior_box", input_prior_box);

    auto outputs = network.execute();

    EXPECT_EQ(outputs.size(), size_t(1));
    EXPECT_EQ(outputs.begin()->first, "detection_output");

    EXPECT_EQ(outputs.begin()->second.get_memory().get_layout().size.batch[0], 1);
    EXPECT_EQ(outputs.begin()->second.get_memory().get_layout().size.feature[0], 1);
    EXPECT_EQ(outputs.begin()->second.get_memory().get_layout().size.spatial[1], keep_top_k * this->num_of_images);
    EXPECT_EQ(outputs.begin()->second.get_memory().get_layout().size.spatial[0], 7);

    auto output_prim = outputs.begin()->second.get_memory();

    this->check_results(output_prim, 0, "0 0 0.6 0.55 0.55 0.85 0.85");
    this->check_results(output_prim, 1, "0 0 0.4 0.15 0.55 0.45 0.85");
    this->check_results(output_prim, 2, "0 1 1.0 0.20 0.20 0.50 0.50");
    this->check_results(output_prim, 3, "0 1 0.8 0.50 0.20 0.80 0.50");
    this->check_results(output_prim, 4, "1 0 1.0 0.25 0.25 0.55 0.55");
    this->check_results(output_prim, 5, "1 1 0.6 0.40 0.40 0.70 0.70");
    this->check_results(output_prim, 6, "-1 0 0 0 0 0 0");
    this->check_results(output_prim, 7, "-1 0 0 0 0 0 0");
}


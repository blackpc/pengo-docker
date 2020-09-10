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
#include "api/CPP/upsampling.hpp"
#include <api/CPP/topology.hpp>
#include <api/CPP/network.hpp>
#include <api/CPP/engine.hpp>
#include "test_utils/test_utils.h"
#include <api/CPP/reorder.hpp>
#include <api/CPP/data.hpp>

using namespace cldnn;
using namespace tests;

TEST(upsampling_gpu, basic_in2x3x2x2_nearest) {
    //  Input  : 2x2x3x2
    //  Output : 2x2x6x4
    //  Sample Type: Nearest

    //  Input:
    //  f0: b0:  1    2  -10   b1:   0    0     -11
    //  f0: b0:  3    4  -14   b1:   0.5 -0.5   -15  
    //  f1: b0:  5    6  -12   b1:   1.5  5.2   -13     
    //  f1: b0:  7    8  -16   b1:   12   9     -17
    //

    engine engine;

    auto input = memory::allocate(engine, { data_types::f32, format::bfyx, { 2, 2, 3, 2 } });

    uint32_t scale = 2;

    topology topology;
    topology.add(input_layout("input", input.get_layout()));
    topology.add(upsampling("upsampling", "input", scale, 0, upsampling_sample_type::nearest));

    set_values(input, {
        1.f, 2.f, -10.f,
        3.f, 4.f, -14.f,
        5.f, 6.f, -12.f,
        7.f, 8.f, -16.f,
        0.f, 0.f, -11.f,
        0.5f, -0.5f, -15.f,
        1.5f, 5.2f, -13.f,
        12.f, 9.f, -17.f,
    });

    network network(engine, topology);

    network.set_input_data("input", input);

    auto outputs = network.execute();

    auto output = outputs.at("upsampling").get_memory();
    auto output_ptr = output.pointer<float>();

    float answers[96] = {
        1.f, 1.f, 2.f,   2.f,   -10.f,  -10.f,
        1.f, 1.f, 2.f,   2.f,   -10.f,  -10.f,
        3.f, 3.f, 4.f,   4.f,   -14.f,  -14.f,
        3.f, 3.f, 4.f,   4.f,   -14.f,  -14.f,
        5.f, 5.f, 6.f,   6.f,   -12.f,  -12.f,
        5.f, 5.f, 6.f,   6.f,   -12.f,  -12.f,
        7.f, 7.f, 8.f,   8.f,   -16.f,  -16.f,
        7.f, 7.f, 8.f,   8.f,   -16.f,  -16.f,
        0.f, 0.f, 0.f,   0.f,   -11.f,  -11.f,
        0.f, 0.f, 0.f,   0.f,   -11.f,  -11.f,
        0.5f,0.5f, -0.5f, -0.5f, -15.f,  -15.f,
        0.5f,0.5f, -0.5f, -0.5f, -15.f,  -15.f,
        1.5f,1.5f, 5.2f,  5.2f,  -13.f,  -13.f,
        1.5f,1.5f, 5.2f,  5.2f,  -13.f,  -13.f,
        12.f,12.f, 9.f,   9.f,  -17.f,  -17.f,
        12.f,12.f, 9.f,   9.f,  -17.f,  -17.f,
    };

    for (int i = 0; i < 2; ++i) { //B
        for (int j = 0; j < 2; ++j) { //F
            for (int k = 0; k < 4; ++k) { //Y
                for (int l = 0; l < 6; ++l) { //X
                    auto linear_id = l + k * 6 + j * 4 * 6 + i * 2 * 4 * 6;
                    EXPECT_TRUE(are_equal(answers[linear_id], output_ptr[linear_id]));
                }
            }
        }
    }
}

TEST(upsampling_gpu, basic_in2x3x2x2_bilinear) {
    //  Input  : 1x1x2x2
    //  Output : 1x1x4x4
    //  Sample Type: Nearest

    //  Input:
    //  f0: b0:  1    2
    //  f0: b0:  3    4
    //

    engine engine;

    auto input = memory::allocate(engine, { data_types::f32, format::bfyx,{ 1, 1, 2, 2 } });

    uint32_t scale = 2;

    topology topology;
    topology.add(input_layout("input", input.get_layout()));
    topology.add(upsampling("upsampling", "input", scale, 1, upsampling_sample_type::bilinear));

    set_values(input, {
        1.f, 2.f,
        3.f, 4.f,
    });

    network network(engine, topology);

    network.set_input_data("input", input);

    auto outputs = network.execute();

    auto output = outputs.at("upsampling").get_memory();
    auto output_ptr = output.pointer<float>();

    float answers[16] = {
        0.5625f, 0.9375f, 1.3125f, 1.125f,
        1.125f, 1.75f, 2.25f, 1.875f,
        1.875f, 2.75f, 3.25f, 2.625f,
        1.6875f, 2.4375f, 2.8125f, 2.25f,
    };

    for (int k = 0; k < 4; ++k) { //Y
        for (int l = 0; l < 4; ++l) { //X
            auto linear_id = l + k * 4;
            EXPECT_NEAR(answers[linear_id], output_ptr[linear_id], 1e-05F);
        }
    }
}
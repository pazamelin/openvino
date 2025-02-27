// Copyright (C) 2018-2022 Intel Corporation
// SPDX-License-Identifier: Apache-2.0
//

#include <gtest/gtest.h>

#include <low_precision/reduce_mean.hpp>
#include <memory>
#include <sstream>
#include <string>
#include <transformations/utils/utils.hpp>
#include <utility>

#include "common_test_utils/ngraph_test_utils.hpp"
#include "lpt_ngraph_functions/common/constant.hpp"
#include "lpt_ngraph_functions/common/dequantization_operations.hpp"
#include "lpt_ngraph_functions/reduce_function.hpp"
#include "reduce_transformation.hpp"
#include "simple_low_precision_transformer.hpp"

namespace {
using namespace testing;
using namespace ngraph;
using namespace ngraph::pass;
using namespace ngraph::builder::subgraph;

class ReduceMeanTransformation : public ReduceTransformation<opset1::ReduceMean> {
    void SetUp() override {
        ReduceTransformation::SetUp();
        const auto transformationParams = std::get<1>(GetParam()).params;

        SimpleLowPrecisionTransformer transform;
        transform.add<ngraph::pass::low_precision::ReduceMeanTransformation, ngraph::opset1::ReduceMean>(
            transformationParams);
        transform.transform(actualFunction);
    }
};

TEST_P(ReduceMeanTransformation, CompareFunctions) {
    actualFunction->validate_nodes_and_infer_types();
    auto res = compare_functions(actualFunction, referenceFunction, true, true, false);
    ASSERT_TRUE(res.first) << res.second;

    ASSERT_TRUE(LayerTransformation::allNamesAreUnique(actualFunction)) << "Not all names are unique";
}

namespace testValues1 {
const std::vector<ngraph::PartialShape> inputShapes = {{1, 3, 16, 16}, {4, 3, 16, 16}, {-1, -1, -1, -1}};

const std::vector<ReduceTransformationTestValues> reduceMeanTransformationTestValues = {
    // U8: keep dims, per-channel quantization, reduction by batch
    {LayerTransformation::createParamsU8I8(),
     {0},
     true,
     {ngraph::element::u8, {{ngraph::element::f32}, {}, {{0.1f, 1.f, 10.f}, ngraph::element::f32, {1, 3, 1, 1}}}},
     {ngraph::element::u8,
      {},
      ngraph::element::f32,
      {{}, {}, {{0.1f, 1.f, 10.f}, ngraph::element::f32, {1, 3, 1, 1}}}}},
    // U8: keep dims, per-channel quantization with subtract, reduction by batch
    {LayerTransformation::createParamsU8I8(),
     {0},
     true,
     {ngraph::element::u8,
      {{ngraph::element::f32},
       {{64.f, 128.f, 32.f}, ngraph::element::f32, {1, 3, 1, 1}},
       {{0.1f, 1.f, 10.f}, ngraph::element::f32, {1, 3, 1, 1}}}},
     {ngraph::element::u8,
      {},
      ngraph::element::f32,
      {{},
       {{64.f, 128.f, 32.f}, ngraph::element::f32, {1, 3, 1, 1}},
       {{0.1f, 1.f, 10.f}, ngraph::element::f32, {1, 3, 1, 1}}}}},
    // U8: don't keep dims, per-channel quantization, reduction by channel
    {LayerTransformation::createParamsU8I8(),
     {1},
     false,
     {ngraph::element::u8, {{ngraph::element::f32}, {}, {{0.1f, 1.f, 10.f}, ngraph::element::f32, {1, 3, 1, 1}}}},
     {ngraph::element::u8,
      {{ngraph::element::f32}, {}, {{0.1f, 1.f, 10.f}, ngraph::element::f32, {1, 3, 1, 1}}},
      ngraph::element::f32,
      {}}},
    // U8: don't keep dims, per-tensor quantization, reduction by channel (reduction constant with negative values)
    {LayerTransformation::createParamsU8I8(),
     {-2},
     false,
     {ngraph::element::u8, {{ngraph::element::f32}, {128.f}, {0.1f}}},
     {ngraph::element::u8, {}, ngraph::element::f32, {{}, {128.f}, {0.1f}}}},
    // U8: keep dims, per-channel quantization, reduction by special dimensions
    {LayerTransformation::createParamsU8I8(),
     {2, 3},
     true,
     {ngraph::element::u8, {{ngraph::element::f32}, {}, {{0.1f, 1.f, 10.f}, ngraph::element::f32, {1, 3, 1, 1}}}},
     {ngraph::element::u8,
      {},
      ngraph::element::f32,
      {{}, {}, {{0.1f, 1.f, 10.f}, ngraph::element::f32, {1, 3, 1, 1}}}}},
    // U8: don't keep dims, per-channel quantization, reduction by special dimensions
    {LayerTransformation::createParamsU8I8(),
     {2, 3},
     false,
     {ngraph::element::u8, {{ngraph::element::f32}, {}, {{0.1f, 1.f, 10.f}, ngraph::element::f32, {1, 3, 1, 1}}}},
     {ngraph::element::u8, {}, ngraph::element::f32, {{}, {}, {{0.1f, 1.f, 10.f}, ngraph::element::f32, {1, 3}}}}},
    // I8: keep dims, per-channel quantization, reduction by batch
    {LayerTransformation::createParamsI8I8(),
     {0},
     true,
     {ngraph::element::i8, {{ngraph::element::f32}, {}, {{0.1f, 1.f, 10.f}, ngraph::element::f32, {1, 3, 1, 1}}}},
     {ngraph::element::i8,
      {},
      ngraph::element::f32,
      {{}, {}, {{0.1f, 1.f, 10.f}, ngraph::element::f32, {1, 3, 1, 1}}}}},
    // I8: don't keep dims, per-channel quantization, reduction by channel
    {LayerTransformation::createParamsI8I8(),
     {1},
     false,
     {ngraph::element::i8, {{ngraph::element::f32}, {}, {{0.1f, 1.f, 10.f}, ngraph::element::f32, {1, 3, 1, 1}}}},
     {ngraph::element::i8,
      {{ngraph::element::f32}, {}, {{0.1f, 1.f, 10.f}, ngraph::element::f32, {1, 3, 1, 1}}},
      ngraph::element::f32,
      {}}},
    // I8: don't keep dims, per-tensor quantization, reduction by channel (reduction constant with negative values)
    {LayerTransformation::createParamsI8I8(),
     {-2},
     false,
     {ngraph::element::i8, {{ngraph::element::f32}, {64.f}, {0.1f}}},
     {ngraph::element::i8, {}, ngraph::element::f32, {{}, {64.f}, {0.1f}}}},
    // I8: don't keep dims, per-channel quantization, reduction by special dimensions
    {LayerTransformation::createParamsI8I8(),
     {2, 3},
     false,
     {ngraph::element::u8, {{ngraph::element::f32}, {}, {{0.1f, 1.f, 10.f}, ngraph::element::f32, {1, 3, 1, 1}}}},
     {ngraph::element::u8, {}, ngraph::element::f32, {{}, {}, {{0.1f, 1.f, 10.f}, ngraph::element::f32, {1, 3}}}}},
    // I8: keep dims, per-channel quantization, reduction by special dimensions
    {LayerTransformation::createParamsI8I8(),
     {2, 3},
     true,
     {ngraph::element::i8, {{ngraph::element::f32}, {}, {{0.1f, 1.f, 10.f}, ngraph::element::f32, {1, 3, 1, 1}}}},
     {ngraph::element::i8,
      {},
      ngraph::element::f32,
      {{}, {}, {{0.1f, 1.f, 10.f}, ngraph::element::f32, {1, 3, 1, 1}}}}},
    // not update precisions, keep dims, per-channel quantization, reduction by special dimensions
    {LayerTransformation::createParamsI8I8().setUpdatePrecisions(false),
     {2, 3},
     true,
     {ngraph::element::f32, {{}, {}, {{0.1f, 1.f, 10.f}, ngraph::element::f32, {1, 3, 1, 1}}}},
     {ngraph::element::f32,
      {},
      ngraph::element::f32,
      {{}, {}, {{0.1f, 1.f, 10.f}, ngraph::element::f32, {1, 3, 1, 1}}}}},
    // I8: keep dims, no dequantization, reduction by special dimensions
    {LayerTransformation::createParamsI8I8(),
     {2, 3},
     true,
     {ngraph::element::f32, {}},
     {ngraph::element::f32, {}, ngraph::element::f32, {}}},
};

INSTANTIATE_TEST_SUITE_P(smoke_LPT,
                         ReduceMeanTransformation,
                         ::testing::Combine(::testing::ValuesIn(inputShapes),
                                            ::testing::ValuesIn(reduceMeanTransformationTestValues)),
                         ReduceMeanTransformation::getTestCaseName);
}  // namespace testValues1

namespace testValues2 {
const std::vector<ngraph::PartialShape> inputShapesWithDynamicRank = {PartialShape::dynamic()};

const std::vector<ReduceTransformationTestValues> reduceMeanTransformationTestValues = {
    {LayerTransformation::createParamsU8I8(),
     {-2},
     false,
     {ngraph::element::u8, {{ngraph::element::f32}, {128.f}, {0.1f}}},
     {ngraph::element::u8, {{ngraph::element::f32}, {128.f}, {0.1f}}, ngraph::element::f32, {}}}};

INSTANTIATE_TEST_SUITE_P(smoke_LPT,
                         ReduceMeanTransformation,
                         ::testing::Combine(::testing::ValuesIn(inputShapesWithDynamicRank),
                                            ::testing::ValuesIn(reduceMeanTransformationTestValues)),
                         ReduceMeanTransformation::getTestCaseName);
}  // namespace testValues2
}  // namespace

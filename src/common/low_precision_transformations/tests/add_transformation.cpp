// Copyright (C) 2018-2022 Intel Corporation
// SPDX-License-Identifier: Apache-2.0
//

#include <gtest/gtest.h>

#include <low_precision/add.hpp>
#include <memory>
#include <sstream>
#include <string>
#include <transformations/init_node_info.hpp>
#include <transformations/utils/utils.hpp>
#include <utility>

#include "common_test_utils/ngraph_test_utils.hpp"
#include "layer_transformation.hpp"
#include "lpt_ngraph_functions/add_function.hpp"
#include "lpt_ngraph_functions/common/dequantization_operations.hpp"
#include "simple_low_precision_transformer.hpp"

namespace {
using namespace testing;
using namespace ngraph;
using namespace ngraph::pass;
using namespace ngraph::builder::subgraph;

class AddTransformationTestValues {
public:
    class Actual {
    public:
        ngraph::element::Type precision1;
        ngraph::builder::subgraph::DequantizationOperations dequantization1;
        ngraph::element::Type precision2;
        ngraph::builder::subgraph::DequantizationOperations dequantization2;
        std::vector<float> constValues;
    };

    class Expected {
    public:
        ngraph::element::Type precision1;
        ngraph::builder::subgraph::DequantizationOperations dequantization1;
        ngraph::element::Type precision2;
        ngraph::builder::subgraph::DequantizationOperations dequantization2;
        ngraph::builder::subgraph::DequantizationOperations dequantizationAfter;
        std::vector<float> constValues;
        std::string operationType;

        Expected() = default;

        Expected(const ngraph::element::Type& precision1,
                 ngraph::builder::subgraph::DequantizationOperations dequantization1,
                 const ngraph::element::Type& precision2,
                 ngraph::builder::subgraph::DequantizationOperations dequantization2,
                 ngraph::builder::subgraph::DequantizationOperations dequantizationAfter,
                 std::vector<float> constValues,
                 std::string operationType = "Add")
            : precision1(precision1),
              dequantization1(std::move(dequantization1)),
              precision2(precision2),
              dequantization2(std::move(dequantization2)),
              dequantizationAfter(std::move(dequantizationAfter)),
              constValues(std::move(constValues)),
              operationType(std::move(operationType)) {}
    };

    bool broadcast;
    int constInput;
    TestTransformationParams params;
    Actual actual;
    Expected expected;
    std::string additionalLayer;
};

typedef std::tuple<ngraph::element::Type,
                   std::pair<ngraph::PartialShape, ngraph::PartialShape>,  // PShapes for each input
                   AddTransformationTestValues>
    AddTransformationParams;

class AddTransformation : public LayerTransformation, public testing::WithParamInterface<AddTransformationParams> {
public:
    void SetUp() override {
        const ngraph::element::Type precision = std::get<0>(GetParam());
        const auto inputShapes = std::get<1>(GetParam());
        const AddTransformationTestValues& testValues = std::get<2>(GetParam());

        actualFunction = AddFunction::getOriginal(precision,
                                                  inputShapes.first,
                                                  inputShapes.second,
                                                  testValues.broadcast,
                                                  TestTransformationParams::toParams(testValues.params),
                                                  testValues.actual.precision1,
                                                  testValues.actual.dequantization1,
                                                  testValues.actual.precision2,
                                                  testValues.actual.dequantization2,
                                                  testValues.constInput,
                                                  testValues.actual.constValues,
                                                  testValues.additionalLayer);

        SimpleLowPrecisionTransformer transform;
        transform.add<ngraph::pass::low_precision::AddTransformation, ngraph::opset1::Add>(testValues.params);
        transform.transform(actualFunction);

        auto inputShape1Ref = inputShapes.first;
        auto inputShape2Ref = inputShapes.second;
        if (testValues.constInput == 0) {
            std::swap(inputShape1Ref, inputShape2Ref);
        }

        referenceFunction = AddFunction::getReference(precision,
                                                      inputShape1Ref,
                                                      inputShape2Ref,
                                                      testValues.broadcast,
                                                      TestTransformationParams::toParams(testValues.params),
                                                      testValues.expected.precision1,
                                                      testValues.expected.dequantization1,
                                                      testValues.expected.precision2,
                                                      testValues.expected.dequantization2,
                                                      testValues.expected.dequantizationAfter,
                                                      // Constant operations after transformations are on 1 input only
                                                      testValues.constInput == -1 ? -1 : 1,
                                                      testValues.expected.constValues,
                                                      testValues.additionalLayer,
                                                      testValues.expected.operationType);
    }

    static std::string getTestCaseName(testing::TestParamInfo<AddTransformationParams> obj) {
        const element::Type precision = std::get<0>(obj.param);
        const auto inputShapes = std::get<1>(obj.param);
        const AddTransformationTestValues testValues = std::get<2>(obj.param);

        std::ostringstream result;
        result << precision << "_" << inputShapes.first << "_" << inputShapes.second << "_" << testValues.broadcast
               << "_" << testValues.actual.precision1 << "_" << testValues.actual.dequantization1 << "_"
               << testValues.actual.precision2 << "_" << testValues.actual.dequantization2 << "_"
               << testValues.constInput << "_" << testValues.actual.constValues << "_" << testValues.additionalLayer
               << "_" << (testValues.params.updatePrecisions ? "true" : "false");
        return result.str();
    }
};

TEST_P(AddTransformation, CompareFunctions) {
    actualFunction->validate_nodes_and_infer_types();
    auto res = compare_functions(actualFunction, referenceFunction, true, true, false);
    ASSERT_TRUE(res.first) << res.second;

    ASSERT_TRUE(LayerTransformation::allNamesAreUnique(actualFunction)) << "Not all names are unique";
}

const std::vector<ngraph::element::Type> netPrecision = {element::f32, element::f16};

namespace testValues1 {
const std::vector<std::pair<ngraph::PartialShape, ngraph::PartialShape>> inputShapes4D = {
    {{1, 4, 16, 16}, {1, 4, 16, 16}},
    {{Dimension::dynamic(), 4, Dimension::dynamic(), Dimension::dynamic()},
     {Dimension::dynamic(), 4, Dimension::dynamic(), Dimension::dynamic()}},
};

const std::vector<AddTransformationTestValues> testValuesWithoutConstantBranches = {
    // Multiply with zero on the first branch
    {false,
     -1,
     LayerTransformation::createParamsU8I8(),
     {ngraph::element::f32, {}, ngraph::element::u8, {{ngraph::element::f32}, {7.f}, {{1.f, 0.f, 2.f, 3.f}}}, {}},
     {ngraph::element::f32, {}, ngraph::element::u8, {{ngraph::element::f32}, {7.f}, {{1.f, 0.f, 2.f, 3.f}}}, {}, {}},
     ""},
    // Multiply with zero on the second branch
    {false,
     -1,
     LayerTransformation::createParamsU8I8(),
     {ngraph::element::u8, {{ngraph::element::f32}, {7.f}, {{1.f, 0.f, 2.f, 3.f}}}, ngraph::element::f32, {}, {}},
     {ngraph::element::u8, {{ngraph::element::f32}, {7.f}, {{1.f, 0.f, 2.f, 3.f}}}, ngraph::element::f32, {}, {}, {}},
     ""},

    // Actual:
    //
    // Parameter           Parameter
    //   |U8                 |U8
    //   |                   |
    // Convert Constant    Convert  Constant
    //  \FP32  /FP32        \FP32   /FP32
    //   \    /              \     /
    //  Subtract  Constant  Subtract  Constant
    //     \FP32   /FP32       \FP32  /FP32
    //      \     /             \    /
    //      Multiply           Multiply
    //             \FP32      /FP32
    //              \        /
    //                 Add
    // Transformed:
    //
    // Parameter
    //   |U8
    //   |
    // Convert  Constant
    //   \FP32   /FP32
    //    \     /
    //   Subtract    Constant
    //      \FP32    /FP32
    //       \      /
    //      Multiply   Parameter
    //          \FP32  /U8
    //           \    /
    //            Add
    {false,
     -1,
     LayerTransformation::createParamsU8I8(),
     {ngraph::element::u8,
      {{ngraph::element::f32}, {7.f}, {10.f}},
      ngraph::element::u8,
      {{ngraph::element::f32}, {3.f}, {5.f}},
      {}},
     {ngraph::element::u8,
      {{ngraph::element::f32}, {8.5f}, {2.f}},
      ngraph::element::u8,
      {{}, {}, {}},
      {{}, {}, {5.f}},
      {}},
     ""},

    // Actual:
    //
    // Parameter Constant Parameter Constant
    //  |U8      |U8        |U8      |U8
    //  |        |          |        |
    // Convert Convert    Convert  Convert
    //  \FP32  /FP32        \FP32   /FP32
    //   \    /              \     /
    //  Subtract  Constant  Subtract  Constant
    //     \FP32   /FP32       \FP32  /FP32
    //      \     /             \    /
    //      Multiply           Multiply
    //             \FP32      /FP32
    //              \        /
    //                 Add
    // Transformed:
    //
    // Parameter
    //   |U8
    //   |
    // Convert  Constant
    //   \FP32   /FP32
    //    \     /
    //   Subtract    Constant
    //      \FP32    /FP32
    //       \      /
    //      Multiply   Parameter
    //          \FP32  /U8
    //           \    /
    //            Add
    {false,
     -1,
     LayerTransformation::createParamsU8I8(),
     {ngraph::element::u8,
      {{ngraph::element::f32}, {{7.f}, ngraph::element::f32, {}, false, 1, ngraph::element::u8, true}, {10.f}},
      ngraph::element::u8,
      {{ngraph::element::f32}, {{3.f}, ngraph::element::f32, {}, false, 1, ngraph::element::u8, true}, {5.f}},
      {}},
     {ngraph::element::u8,
      {{ngraph::element::f32}, {8.5f}, {2.f}},
      ngraph::element::u8,
      {{}, {}, {}},
      {{}, {}, {5.f}},
      {}},
     ""},
    {false,
     -1,
     LayerTransformation::createParamsU8I8(),
     {ngraph::element::u8,
      {{ngraph::element::f32}, {2.f}, {10.f}},
      ngraph::element::u8,
      {{ngraph::element::f32}, {}, {5.f}},
      {}},
     {ngraph::element::u8,
      {{ngraph::element::f32}, {2.f}, {2.f}},
      ngraph::element::u8,
      {{}, {}, {}},
      {{}, {}, {5.f}},
      {}},
     ""},
    {false,
     -1,
     LayerTransformation::createParamsU8I8(),
     {ngraph::element::u8,
      {{ngraph::element::f32}, {}, {10.f}},
      ngraph::element::u8,
      {{ngraph::element::f32}, {}, {5.f}},
      {}},
     {ngraph::element::u8, {{ngraph::element::f32}, {}, {2.f}}, ngraph::element::u8, {{}, {}, {}}, {{}, {}, {5.f}}, {}},
     ""},
    {false,
     -1,
     LayerTransformation::createParamsU8I8(),
     {ngraph::element::u8,
      {{ngraph::element::f32}, {2.f}, {}},
      ngraph::element::u8,
      {{ngraph::element::f32}, {}, {5.f}},
      {}},
     {ngraph::element::u8,
      {{ngraph::element::f32}, {2.f}, {0.2f}},
      ngraph::element::u8,
      {{}, {}, {}},
      {{}, {}, {5.f}},
      {}},
     ""},
    {false,
     -1,
     LayerTransformation::createParamsU8I8(),
     {ngraph::element::u8,
      {{ngraph::element::f32}, {2.f}, {}},
      ngraph::element::u8,
      {{ngraph::element::f32}, {3.f}, {5.f}},
      {}},
     {ngraph::element::u8,
      {{ngraph::element::f32}, {17.f}, {0.2f}},
      ngraph::element::u8,
      {{}, {}, {}},
      {{}, {}, {5.f}},
      {}},
     ""},

    // I8 + broadcast

    {true,
     -1,
     LayerTransformation::createParamsU8I8(),
     {ngraph::element::i8,
      {{ngraph::element::f32}, {7.f}, {10.f}},
      ngraph::element::i8,
      {{ngraph::element::f32}, {3.f}, {5.f}},
      {}},
     {ngraph::element::i8,
      {{ngraph::element::f32}, {8.5f}, {2.f}},
      ngraph::element::i8,
      {{}, {}, {}},
      {{}, {}, {5.f}},
      {}},
     ""},
    {true,
     -1,
     LayerTransformation::createParamsU8I8(),
     {ngraph::element::i8,
      {{ngraph::element::f32}, {2.f}, {10.f}},
      ngraph::element::i8,
      {{ngraph::element::f32}, {}, {5.f}},
      {}},
     {ngraph::element::i8,
      {{ngraph::element::f32}, {2.f}, {2.f}},
      ngraph::element::i8,
      {{}, {}, {}},
      {{}, {}, {5.f}},
      {}},
     ""},
    {true,
     -1,
     LayerTransformation::createParamsU8I8(),
     {ngraph::element::i8,
      {{ngraph::element::f32}, {}, {10.f}},
      ngraph::element::i8,
      {{ngraph::element::f32}, {}, {5.f}},
      {}},
     {ngraph::element::i8, {{ngraph::element::f32}, {}, {2.f}}, ngraph::element::i8, {{}, {}, {}}, {{}, {}, {5.f}}, {}},
     ""},
    {true,
     -1,
     LayerTransformation::createParamsU8I8(),
     {ngraph::element::i8,
      {{ngraph::element::f32}, {2.f}, {}},
      ngraph::element::i8,
      {{ngraph::element::f32}, {}, {5.f}},
      {}},
     {ngraph::element::i8,
      {{ngraph::element::f32}, {2.f}, {0.2f}},
      ngraph::element::i8,
      {{}, {}, {}},
      {{}, {}, {5.f}},
      {}},
     ""},
    {true,
     -1,
     LayerTransformation::createParamsU8I8(),
     {ngraph::element::i8,
      {{ngraph::element::f32}, {2.f}, {}},
      ngraph::element::i8,
      {{ngraph::element::f32}, {3.f}, {5.f}},
      {}},
     {ngraph::element::i8,
      {{ngraph::element::f32}, {17.f}, {0.2f}},
      ngraph::element::i8,
      {{}, {}, {}},
      {{}, {}, {5.f}},
      {}},
     ""},

    // convolution before FQ (choose that branch)
    {false,
     -1,
     LayerTransformation::createParamsU8I8(),
     {ngraph::element::u8,
      {{ngraph::element::f32}, {7.f}, {10.f}},
      ngraph::element::u8,
      {{ngraph::element::f32}, {3.f}, {5.f}},
      {}},
     {ngraph::element::u8,
      {{}, {}, {}},
      ngraph::element::u8,
      {{ngraph::element::f32}, {17.f}, {0.5f}},
      {{}, {}, {10.f}},
      {}},
     "convolution"},
    // convolution with multiple consumers before FQ ( FP32 on other branch due to possible quantize fusing )
    {false,
     -1,
     LayerTransformation::createParamsU8I8(),
     {ngraph::element::u8,
      {{ngraph::element::f32}, {7.f}, {10.f}},
      ngraph::element::u8,
      {{ngraph::element::f32}, {3.f}, {5.f}},
      {}},
     {ngraph::element::u8,
      {{ngraph::element::f32}, {8.5f}, {2.f}},
      ngraph::element::u8,
      {{}, {}, {}},
      {{}, {}, {5.f}},
      {}},
     "convolution_multiconsumers"},
    // group convolution before FQ (choose that branch)
    {false,
     -1,
     LayerTransformation::createParamsU8I8(),
     {ngraph::element::u8,
      {{ngraph::element::f32}, {7.f}, {10.f}},
      ngraph::element::u8,
      {{ngraph::element::f32}, {3.f}, {5.f}},
      {}},
     {ngraph::element::u8,
      {{}, {}, {}},
      ngraph::element::u8,
      {{ngraph::element::f32}, {17.f}, {0.5f}},
      {{}, {}, {10.f}},
      {}},
     "group_convolution"},
};

INSTANTIATE_TEST_SUITE_P(smoke_LPT,
                         AddTransformation,
                         ::testing::Combine(::testing::ValuesIn(netPrecision),
                                            ::testing::ValuesIn(inputShapes4D),
                                            ::testing::ValuesIn(testValuesWithoutConstantBranches)),
                         AddTransformation::getTestCaseName);
}  // namespace testValues1

namespace testValues2 {
const std::vector<std::pair<ngraph::PartialShape, ngraph::PartialShape>> inputShapes4D = {
    {{1, 4, 16, 16}, {1, 4, 16, 16}},
    {{1, 4, 16, 16}, {Dimension::dynamic(), 4, Dimension::dynamic(), Dimension::dynamic()}},
};

const std::vector<AddTransformationTestValues> testValuesWithFirstConstantBranch{
    // Actual:
    //
    // Constant Constant   Parameter
    //  |U8      |U8        |U8
    //  |        |          |
    // Convert Convert    Convert  Constant
    //  \FP32  /FP32        \FP32   /FP32
    //   \    /              \     /
    //  Subtract  Constant  Subtract  Constant
    //     \FP32   /FP32       \FP32  /FP32
    //      \     /             \    /
    //      Multiply           Multiply
    //             \FP32      /FP32
    //              \        /
    //                 Add
    // Transformed:
    //
    // Parameter
    //   |U8
    //   |
    // Convert  Constant
    //   \FP32   /FP32
    //    \     /
    //   Subtract    Constant
    //      \FP32    /FP32
    //       \      /
    //      Multiply
    {false,
     0,
     LayerTransformation::createParamsU8I8(),
     {ngraph::element::u8,
      {{ngraph::element::f32}, {{7.f}, ngraph::element::f32, {}, false, 1, ngraph::element::u8, true}, {10.f}},
      ngraph::element::u8,
      {{ngraph::element::f32}, {3.f}, {5.f}},
      {10.f}},
     {ngraph::element::u8,
      {{ngraph::element::f32}, {}, {}},
      ngraph::element::u8,
      {},
      {{}, {}, {5.f}},
      {-3.f},
      "Subtract"},
     ""}};

INSTANTIATE_TEST_SUITE_P(smoke_LPT,
                         AddTransformation,
                         ::testing::Combine(::testing::ValuesIn(netPrecision),
                                            ::testing::ValuesIn(inputShapes4D),
                                            ::testing::ValuesIn(testValuesWithFirstConstantBranch)),
                         AddTransformation::getTestCaseName);
}  // namespace testValues2

namespace testValues3 {
const std::vector<std::pair<ngraph::PartialShape, ngraph::PartialShape>> inputShapes4D = {
    {{1, 4, 16, 16}, {1, 4, 16, 16}},
    {{Dimension::dynamic(), 4, Dimension::dynamic(), Dimension::dynamic()}, {1, 4, 16, 16}},
};

const std::vector<AddTransformationTestValues> testValuesWithSecondConstantBranch = {
    // Actual:
    //
    // Parameter          Parameter Constant
    //  |U8                 |U8      |U8
    //  |                   |        |
    // Convert Constant    Convert  Convert
    //  \FP32  /FP32        \FP32   /FP32
    //   \    /              \     /
    //  Subtract  Constant  Subtract  Constant
    //     \FP32   /FP32       \FP32  /FP32
    //      \     /             \    /
    //      Multiply           Multiply
    //             \FP32      /FP32
    //              \        /
    //                 Add
    // Transformed:
    //
    // Parameter
    //   |U8
    //   |
    // Convert  Constant
    //   \FP32   /FP32
    //    \     /
    //   Subtract    Constant
    //      \FP32    /FP32
    //       \      /
    //      Multiply
    {false,
     1,
     LayerTransformation::createParamsU8I8(),
     {ngraph::element::u8,
      {{ngraph::element::f32}, {7.f}, {10.f}},
      ngraph::element::u8,
      {{ngraph::element::f32}, {{3.f}, ngraph::element::f32, {}, false, 1, ngraph::element::u8, true}, {5.f}},
      {10.f}},
     {ngraph::element::u8,
      {{ngraph::element::f32}, {}, {}},
      ngraph::element::u8,
      {},
      {{}, {}, {10.f}},
      {3.5f},
      "Subtract"},
     ""},
};

INSTANTIATE_TEST_SUITE_P(smoke_LPT,
                         AddTransformation,
                         ::testing::Combine(::testing::ValuesIn(netPrecision),
                                            ::testing::ValuesIn(inputShapes4D),
                                            ::testing::ValuesIn(testValuesWithSecondConstantBranch)),
                         AddTransformation::getTestCaseName);
}  // namespace testValues3

namespace spatialDimensions {
const std::vector<std::pair<ngraph::PartialShape, ngraph::PartialShape>> inputShapes4D = {
    {{1, 2, 2, 2}, {1, 2, 2, 2}},
};

const std::vector<AddTransformationTestValues> specialTestValues = {
    // constant input: Add -> Subtract
    {false,
     1,
     LayerTransformation::createParamsU8I8(),
     {ngraph::element::i8,
      {{ngraph::element::f32}, {}, {5.f}},
      ngraph::element::i8,
      {{}, {}, {}},
      {10.f, 5.f, 2.f, 4.f, 3.f, 12.f, 8.f, 14.f}},
     {ngraph::element::i8,
      {{ngraph::element::f32}, {}, {}},
      ngraph::element::f32,
      {{}, {}, {}},
      {{}, {}, {5.f}},
      {-2.f, -1.f, -0.4f, -0.8f, -0.6f, -2.4f, -1.6f, -2.8f},
      "Subtract"},
     ""},

    // constant input: Add -> Subtract
    {
        false,
        0,
        LayerTransformation::createParamsU8I8(),
        {ngraph::element::i8,
         {{}, {}, {}},
         ngraph::element::i8,
         {{ngraph::element::f32}, {}, {5.f}},
         {10.f, 5.f, 2.f, 4.f, 3.f, 12.f, 8.f, 14.f}},
        {ngraph::element::i8,
         {{ngraph::element::f32}, {}, {}},
         ngraph::element::f32,
         {{}, {}, {}},

         {{}, {}, {5.f}},
         {-2.f, -1.f, -0.4f, -0.8f, -0.6f, -2.4f, -1.6f, -2.8f},
         "Subtract"},
        "",
    },
};

INSTANTIATE_TEST_SUITE_P(smoke_LPT,
                         AddTransformation,
                         ::testing::Combine(::testing::ValuesIn(netPrecision),
                                            ::testing::ValuesIn(inputShapes4D),
                                            ::testing::ValuesIn(specialTestValues)),
                         AddTransformation::getTestCaseName);
}  // namespace spatialDimensions

namespace tensor2D {
const std::vector<std::pair<ngraph::PartialShape, ngraph::PartialShape>> inputShapes = {
    {{4, 1}, {4, 1}},
};

const std::vector<AddTransformationTestValues> specialTestValues = {
    {false,
     -1,
     LayerTransformation::createParamsU8I8(),
     {ngraph::element::u8,
      {{ngraph::element::f32}, {}, {{1.f, 2.f, 3.f, 4.f}, ngraph::element::f32, {4, 1}, true, 0ul}},
      ngraph::element::f32,
      {},
      {5.f, 6.f, 7.f, 8.f}},
     {ngraph::element::u8,
      {{ngraph::element::f32}, {}, {{1.f, 2.f, 3.f, 4.f}, ngraph::element::f32, {4, 1}, true, 0ul}},
      ngraph::element::f32,
      {{}, {}, {}},
      {{}, {}, {}},
      {5.f, 6.f, 7.f, 8.f}},
     ""},
};

INSTANTIATE_TEST_SUITE_P(smoke_LPT,
                         AddTransformation,
                         ::testing::Combine(::testing::ValuesIn(netPrecision),
                                            ::testing::ValuesIn(inputShapes),
                                            ::testing::ValuesIn(specialTestValues)),
                         AddTransformation::getTestCaseName);
}  // namespace tensor2D

namespace oneBranchQuantizationFp16 {
const std::vector<ngraph::element::Type> netPrecision = {element::f16};

const std::vector<std::pair<ngraph::PartialShape, ngraph::PartialShape>> inputShapesWithDynamicChannels = {
    {{1, 4, 16, 16}, {1, 4, 16, 16}},
    {{1, 4, 16, 16}, {Dimension::dynamic(), Dimension::dynamic(), Dimension::dynamic(), Dimension::dynamic()}},
    {{Dimension::dynamic(), Dimension::dynamic(), Dimension::dynamic(), Dimension::dynamic()},
     {Dimension::dynamic(), Dimension::dynamic(), Dimension::dynamic(), Dimension::dynamic()}}};

const std::vector<AddTransformationTestValues> testValues = {
    {false,
     -1,
     LayerTransformation::createParamsU8I8(),
     {ngraph::element::f32, {}, ngraph::element::u8, {{ngraph::element::f32}, {127.f}, {4.f}}, {}},
     {ngraph::element::f32, {{ngraph::element::f32}, {508.f}, {0.25f}}, ngraph::element::u8, {}, {{}, {}, {4.f}}, {}},
     ""},
    {false,
     -1,
     LayerTransformation::createParamsU8I8(),
     {ngraph::element::u8, {{ngraph::element::f32}, {127.f}, {4.f}}, ngraph::element::f32, {}, {}},
     {ngraph::element::u8, {}, ngraph::element::f32, {{ngraph::element::f32}, {508.f}, {0.25f}}, {{}, {}, {4.f}}, {}},
     ""},
    // multiply with zero
    {false,
     -1,
     LayerTransformation::createParamsU8I8(),
     {ngraph::element::f32,
      {},
      ngraph::element::u8,
      {{ngraph::element::f32}, {{7.f, 8.f, 9.f, 10.f}}, {{1.f, 0.f, 2.f, 3.f}}},
      {}},
     {ngraph::element::f32,
      {},
      ngraph::element::u8,
      {{ngraph::element::f32}, {{7.f, 8.f, 9.f, 10.f}}, {{1.f, 0.f, 2.f, 3.f}}},
      {},
      {}},
     ""},
    // float path without subtract
    {false,
     -1,
     LayerTransformation::createParamsU8I8(),
     {ngraph::element::f32, {}, ngraph::element::u8, {{ngraph::element::f32}, {}, {4.f}}, {}},
     {ngraph::element::f32, {{ngraph::element::f32}, {}, {0.25f}}, ngraph::element::u8, {}, {{}, {}, {4.f}}, {}},
     ""},
};

INSTANTIATE_TEST_SUITE_P(smoke_LPT,
                         AddTransformation,
                         ::testing::Combine(::testing::ValuesIn(netPrecision),
                                            ::testing::ValuesIn(inputShapesWithDynamicChannels),
                                            ::testing::ValuesIn(testValues)),
                         AddTransformation::getTestCaseName);
}  // namespace oneBranchQuantizationFp16

namespace oneBranchQuantizationFp32 {
const std::vector<ngraph::element::Type> netPrecision = {
    element::f32,
};

const std::vector<std::pair<ngraph::PartialShape, ngraph::PartialShape>> inputShapesWithDynamicChannels = {
    {{1, 4, 16, 16}, {1, 4, 16, 16}},
    {{1, 4, 16, 16}, {Dimension::dynamic(), Dimension::dynamic(), Dimension::dynamic(), Dimension::dynamic()}},
    {{Dimension::dynamic(), Dimension::dynamic(), Dimension::dynamic(), Dimension::dynamic()},
     {Dimension::dynamic(), Dimension::dynamic(), Dimension::dynamic(), Dimension::dynamic()}}};

const std::vector<AddTransformationTestValues> testValues = {
    {false,
     -1,
     LayerTransformation::createParamsU8I8(),
     {ngraph::element::f32, {}, ngraph::element::u8, {{ngraph::element::f32}, {127.f}, {4.f}}, {}},
     {ngraph::element::f32, {{}, {508.f}, {0.25f}}, ngraph::element::u8, {}, {{}, {}, {4.f}}, {}},
     ""},
    {false,
     -1,
     LayerTransformation::createParamsU8I8(),
     {ngraph::element::u8, {{ngraph::element::f32}, {127.f}, {4.f}}, ngraph::element::f32, {}, {}},
     {ngraph::element::u8, {}, ngraph::element::f32, {{}, {508.f}, {0.25f}}, {{}, {}, {4.f}}, {}},
     ""},
    // multiply with zero
    {false,
     -1,
     LayerTransformation::createParamsU8I8(),
     {ngraph::element::f32,
      {},
      ngraph::element::u8,
      {{ngraph::element::f32}, {{7.f, 8.f, 9.f, 10.f}}, {{1.f, 0.f, 2.f, 3.f}}},
      {}},
     {ngraph::element::f32,
      {},
      ngraph::element::u8,
      {{ngraph::element::f32}, {{7.f, 8.f, 9.f, 10.f}}, {{1.f, 0.f, 2.f, 3.f}}},
      {},
      {}},
     ""},
};

INSTANTIATE_TEST_SUITE_P(smoke_LPT,
                         AddTransformation,
                         ::testing::Combine(::testing::ValuesIn(netPrecision),
                                            ::testing::ValuesIn(inputShapesWithDynamicChannels),
                                            ::testing::ValuesIn(testValues)),
                         AddTransformation::getTestCaseName);
}  // namespace oneBranchQuantizationFp32

namespace oneBranchQuantizationFp32DontUpdatePrecision {
const std::vector<ngraph::element::Type> netPrecision = {
    element::f32,
};

const std::vector<std::pair<ngraph::PartialShape, ngraph::PartialShape>> inputShapesWithDynamicChannels = {
    {{1, 4, 16, 16}, {1, 4, 16, 16}},
    {{1, 4, 16, 16}, {Dimension::dynamic(), Dimension::dynamic(), Dimension::dynamic(), Dimension::dynamic()}},
    {{Dimension::dynamic(), Dimension::dynamic(), Dimension::dynamic(), Dimension::dynamic()},
     {Dimension::dynamic(), Dimension::dynamic(), Dimension::dynamic(), Dimension::dynamic()}}};

const std::vector<AddTransformationTestValues> testValues = {
    // FP32 model, quantized branch: 1
    {false,
     -1,
     LayerTransformation::createParamsU8I8().setUpdatePrecisions(false),
     {ngraph::element::f32, {}, ngraph::element::f32, {{ngraph::element::f32}, {127.f}, {4.f}}, {}},
     {ngraph::element::f32, {{}, {508.f}, {0.25f}}, ngraph::element::f32, {}, {{}, {}, {4.f}}, {}},
     ""},
    // FP32 model, quantized branch: 0
    {false,
     -1,
     LayerTransformation::createParamsU8I8().setUpdatePrecisions(false),
     {ngraph::element::f32, {{ngraph::element::f32}, {127.f}, {4.f}}, ngraph::element::f32, {}, {}},
     {ngraph::element::f32, {}, ngraph::element::f32, {{}, {508.f}, {0.25f}}, {{}, {}, {4.f}}, {}},
     ""},
    // INT8 model (FQ decomposition before LPT), quantized branch: 1
    {false,
     -1,
     LayerTransformation::createParamsU8I8().setUpdatePrecisions(false),
     {ngraph::element::f32, {}, ngraph::element::u8, {{ngraph::element::f32}, {127.f}, {4.f}}, {}},
     {ngraph::element::f32, {{}, {508.f}, {0.25f}}, ngraph::element::u8, {}, {{}, {}, {4.f}}, {}},
     ""},
    // INT8 model (FQ decomposition before LPT), quantized branch: 0
    {false,
     -1,
     LayerTransformation::createParamsU8I8().setUpdatePrecisions(false),
     {ngraph::element::u8, {{ngraph::element::f32}, {127.f}, {4.f}}, ngraph::element::f32, {}, {}},
     {ngraph::element::u8, {}, ngraph::element::f32, {{}, {508.f}, {0.25f}}, {{}, {}, {4.f}}, {}},
     ""}};

INSTANTIATE_TEST_SUITE_P(smoke_LPT,
                         AddTransformation,
                         ::testing::Combine(::testing::ValuesIn(netPrecision),
                                            ::testing::ValuesIn(inputShapesWithDynamicChannels),
                                            ::testing::ValuesIn(testValues)),
                         AddTransformation::getTestCaseName);
}  // namespace oneBranchQuantizationFp32DontUpdatePrecision
}  // namespace

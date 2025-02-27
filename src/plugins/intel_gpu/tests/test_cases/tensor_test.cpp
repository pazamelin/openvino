// Copyright (C) 2018-2022 Intel Corporation
// SPDX-License-Identifier: Apache-2.0
//

#include "test_utils.h"

TEST(tensor_api, order_new_notation)
{
    cldnn::tensor test{ cldnn::feature(3), cldnn::batch(4), cldnn::spatial(2, 1) };

    //sizes
    ASSERT_EQ(test.batch.size(), size_t(1));
    ASSERT_EQ(test.feature.size(), size_t(1));
    ASSERT_EQ(test.spatial.size(), size_t(cldnn::tensor_spatial_dim_max));

    //passed values
    ASSERT_EQ(test.spatial[0], cldnn::tensor::value_type(2));
    ASSERT_EQ(test.spatial[1], cldnn::tensor::value_type(1));
    ASSERT_EQ(test.feature[0], cldnn::tensor::value_type(3));
    ASSERT_EQ(test.batch[0], cldnn::tensor::value_type(4));

    //reverse
    auto sizes = test.sizes();
    ASSERT_EQ(sizes[0], cldnn::tensor::value_type(4));
    ASSERT_EQ(sizes[1], cldnn::tensor::value_type(3));
    ASSERT_EQ(sizes[2], cldnn::tensor::value_type(2));
    ASSERT_EQ(sizes[3], cldnn::tensor::value_type(1));
}

TEST(tensor_api, order_new_notation_feature_default)
{
    cldnn::tensor test{ cldnn::feature(3), cldnn::spatial(2) };

    //sizes
    ASSERT_EQ(test.batch.size(), size_t(1));
    ASSERT_EQ(test.feature.size(), size_t(1));
    ASSERT_EQ(test.spatial.size(), size_t(cldnn::tensor_spatial_dim_max));

    //passed values
    ASSERT_EQ(test.spatial[0], cldnn::tensor::value_type(2));
    ASSERT_EQ(test.spatial[1], cldnn::tensor::value_type(1));
    ASSERT_EQ(test.feature[0], cldnn::tensor::value_type(3));
    ASSERT_EQ(test.batch[0], cldnn::tensor::value_type(1));

    //reverse
    auto sizes = test.sizes();
    ASSERT_EQ(sizes[0], cldnn::tensor::value_type(1));
    ASSERT_EQ(sizes[1], cldnn::tensor::value_type(3));
    ASSERT_EQ(sizes[2], cldnn::tensor::value_type(2));
    ASSERT_EQ(sizes[3], cldnn::tensor::value_type(1));
}

TEST(tensor_api, order)
{
    cldnn::tensor test{ 1, 2, 3, 4 };

    //sizes
    ASSERT_EQ(test.batch.size(), size_t(1));
    ASSERT_EQ(test.feature.size(), size_t(1));
    ASSERT_EQ(test.spatial.size(), size_t(cldnn::tensor_spatial_dim_max));

    //passed values
    ASSERT_EQ(test.spatial[1], cldnn::tensor::value_type(4));
    ASSERT_EQ(test.spatial[0], cldnn::tensor::value_type(3));
    ASSERT_EQ(test.feature[0], cldnn::tensor::value_type(2));
    ASSERT_EQ(test.batch[0], cldnn::tensor::value_type(1));

    //reverse
    auto sizes = test.sizes();
    ASSERT_EQ(sizes[0], cldnn::tensor::value_type(1));
    ASSERT_EQ(sizes[1], cldnn::tensor::value_type(2));
    ASSERT_EQ(sizes[2], cldnn::tensor::value_type(3));
    ASSERT_EQ(sizes[3], cldnn::tensor::value_type(4));
}

static void test_tensor_offset(cldnn::tensor shape, cldnn::tensor coord, cldnn::format fmt, size_t ref_offset) {
    auto offset = shape.get_linear_offset(coord, fmt);
    ASSERT_EQ(ref_offset, offset)
        << "format: " << fmt << ", shape: " << shape << ", coord: " << coord;
}

TEST(tensor_api, linear_offsets) {
    // Simple formats:
    test_tensor_offset({ 2, 5, 4, 3 }, { 1, 3, 1, 2 }, cldnn::format::bfyx, 105);
    test_tensor_offset({ 2, 5, 4, 3 }, { 1, 3, 1, 2 }, cldnn::format::yxfb, 97);
    test_tensor_offset({ 2, 5, 4, 3 }, { 1, 3, 1, 2 }, cldnn::format::fyxb, 91);
    test_tensor_offset({ 2, 5, 4, 3 }, { 1, 3, 1, 2 }, cldnn::format::byxf, 108);
    test_tensor_offset({ 2, 5, 4, 3, 5 }, { 1, 3, 1, 2, 4 }, cldnn::format::bfzyx, 537);

    // Blocked formats:
    test_tensor_offset({ 2, 5, 4, 3 }, { 1, 3, 1, 2 }, cldnn::format::b_fs_yx_fsv16, 339);
    test_tensor_offset({ 2, 19, 4, 3 }, { 1, 18, 3, 2 }, cldnn::format::b_fs_yx_fsv16, 754);
    test_tensor_offset({ 2, 5, 4, 3 }, { 1, 3, 1, 2 }, cldnn::format::fs_b_yx_fsv32, 675);
    test_tensor_offset({ 2, 37, 4, 3 }, { 1, 35, 3, 2 }, cldnn::format::fs_b_yx_fsv32, 1507);
}

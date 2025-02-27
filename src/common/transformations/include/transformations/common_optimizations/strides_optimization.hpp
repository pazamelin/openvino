// Copyright (C) 2018-2022 Intel Corporation
// SPDX-License-Identifier: Apache-2.0
//

#pragma once

#include <memory>
#include <openvino/pass/graph_rewrite.hpp>
#include <transformations_visibility.hpp>

namespace ov {
namespace pass {

class TRANSFORMATIONS_API ConvStridesPropagation;
class TRANSFORMATIONS_API SupportedNodesStridesPropagation;
class TRANSFORMATIONS_API UnsupportedNodesStridesPropagation;
class TRANSFORMATIONS_API StridesOptimization;

}  // namespace pass
}  // namespace ov

/**
 * @ingroup ie_transformation_common_api
 * @brief ConvStridesPropagation either propagates stride (greater than 1) from Convolution up through the graph
 * or inserts pooling between current node and its consumers if the consumers have different StridesProp attributes.
 * Strides can be propagated if Convolution kernel is {1, 1, ...}
 */
class ov::pass::ConvStridesPropagation : public ov::pass::MatcherPass {
public:
    OPENVINO_RTTI("ConvStridesPropagation", "0");
    ConvStridesPropagation();
};

/**
 * @ingroup ie_transformation_common_api
 * @brief SupportedNodesStridesPropagation either propagates stride (greater than 1) from current node up through the
 * graph or inserts pooling between current node and its consumers if the consumers have different StridesProp
 * attributes.
 */
class ov::pass::SupportedNodesStridesPropagation : public ov::pass::MatcherPass {
public:
    OPENVINO_RTTI("SupportedNodesStridesPropagation", "0");
    SupportedNodesStridesPropagation();
};

/**
 * @ingroup ie_transformation_common_api
 * @brief UnsupportedNodesStridesPropagation inserts pooling between current node and its consumers
 * if the consumers have different StridesProp attributes.
 */
class ov::pass::UnsupportedNodesStridesPropagation : public ov::pass::MatcherPass {
public:
    OPENVINO_RTTI("UnsupportedNodesStridesPropagation", "0");
    UnsupportedNodesStridesPropagation();
};

/**
 * @ingroup ie_transformation_common_api
 * @brief StridesOptimization transformation works backward on function and propagates strides up through the graph if
 * possible
 */
class ov::pass::StridesOptimization : public ov::pass::BackwardGraphRewrite {
public:
    OPENVINO_RTTI("StridesOptimization", "0");
    StridesOptimization();
};

namespace ngraph {
namespace pass {
using ov::pass::ConvStridesPropagation;
using ov::pass::StridesOptimization;
using ov::pass::SupportedNodesStridesPropagation;
using ov::pass::UnsupportedNodesStridesPropagation;
}  // namespace pass
}  // namespace ngraph

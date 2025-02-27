// Copyright (C) 2018-2022 Intel Corporation
// SPDX-License-Identifier: Apache-2.0
//

#pragma once

#include <ie_common.h>
#include <node.h>

namespace ov {
namespace intel_cpu {
namespace node {

class Math : public Node {
public:
    Math(const std::shared_ptr<ngraph::Node>& op, const dnnl::engine& eng, WeightsSharing::Ptr &cache);

    void getSupportedDescriptors() override {};
    void initSupportedPrimitiveDescriptors() override;
    void execute(dnnl::stream strm) override;
    bool created() const override;

    bool needPrepareParams() const override { return false; };
    void executeDynamicImpl(dnnl::stream strm) override;

    static bool isSupportedOperation(const std::shared_ptr<const ngraph::Node>& op, std::string& errorMessage) noexcept;

private:
    static std::map<const ngraph::DiscreteTypeInfo, std::function<void(const std::shared_ptr<ngraph::Node>&, Math& node)>> initializers;

    float alpha = 0.0f;
    float beta = 0.0f;
    float gamma = 0.0f;

    std::string errorPrefix;
};

}   // namespace node
}   // namespace intel_cpu
}   // namespace ov

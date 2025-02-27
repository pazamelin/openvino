// Copyright (C) 2018-2022 Intel Corporation
// SPDX-License-Identifier: Apache-2.0
//

#include "intel_gpu/plugin/program.hpp"
#include "intel_gpu/plugin/common_utils.hpp"

#include "ngraph/op/normalize_l2.hpp"
#include "ngraph/op/constant.hpp"

#include "intel_gpu/primitives/normalize.hpp"
#include "intel_gpu/primitives/data.hpp"

namespace ov {
namespace intel_gpu {

static void CreateNormalizeL2Op(Program& p, const std::shared_ptr<ngraph::op::v0::NormalizeL2>& op) {
    validate_inputs_count(op, {2});
    auto inputs = p.GetInputInfo(op);
    std::string layerName = layer_type_name_ID(op);

    // params
    auto const_axis = std::dynamic_pointer_cast<ngraph::op::v0::Constant>(op->get_input_node_shared_ptr(1));
    if (!const_axis)
        IE_THROW() << "Unsupported axis node type in " << op->get_friendly_name() << " (" << op->get_type_name() << ")";

    auto axis = const_axis->cast_vector<size_t>();
    bool across_spatial = !(axis.size() == 1 && axis[0] == 1);
    float eps = op->get_eps();

    // WA for MO outputting %.6f
    if (eps == 0.0f) {
        eps = 1e-10f;
    }

    // We create fake scale constant and fill it with ones to keep the same behavior as current primitive
    auto scale = std::make_shared<ngraph::op::v0::Constant>(op->get_output_element_type(0), ngraph::Shape{1}, std::vector<float>{1.0});
    cldnn::layout constLayout = cldnn::layout(cldnn::element_type_to_data_type(op->get_output_element_type(0)), cldnn::format::bfyx, cldnn::tensor{1});
    auto mem = p.GetEngine().allocate_memory(constLayout, false);
    cldnn::mem_lock<int8_t> tmpPointer{mem, p.GetEngine().get_program_stream()};
    auto buf = tmpPointer.data();
    auto bufSize = scale->get_output_tensor(0).size();

    if (bufSize != constLayout.bytes_count())
        IE_THROW() << "Invalid scales buffer in NormalizeL2 op " << op->get_friendly_name();

    std::memcpy(&buf[0], scale->get_data_ptr(), bufSize);
    auto scalesName = layerName + "_cldnn_input_scales";
    p.add_primitive(*op, cldnn::data(scalesName, mem));

    auto normPrim = cldnn::normalize(layerName,
                                     inputs[0],
                                     scalesName,
                                     across_spatial,
                                     eps);

    p.add_primitive(*op, normPrim);
}

REGISTER_FACTORY_IMPL(v0, NormalizeL2);

}  // namespace intel_gpu
}  // namespace ov

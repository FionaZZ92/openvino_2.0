// Copyright (C) 2018-2023 Intel Corporation
// SPDX-License-Identifier: Apache-2.0
//

#pragma once
#include "primitive.hpp"
#include "openvino/op/util/attr_types.hpp"

#include <algorithm>
#include <vector>

namespace cldnn {

/// @brief Finds the index of the k max values of input.
/// @details Returns indices in f32, because we currently does not support int32 data type.
/// We use f32, as bigger indices could not fit in smaller data types.
/// If you want to use output as indices outside of network (inside just use lookup table primitive),
/// you will need to firstly cast it to int (look into tests for example).
struct arg_max_min : public primitive_base<arg_max_min> {
    CLDNN_DECLARE_PRIMITIVE(arg_max_min)

    arg_max_min() : primitive_base("", {}),
                    mode(ov::op::TopKMode::MAX),
                    top_k(0),
                    axis(0),
                    sort(ov::op::TopKSortType::NONE),
                    values_first(false) {}

    DECLARE_OBJECT_TYPE_SERIALIZATION

    /// @brief Constructs arg_max_min primitive.
    /// @param id This primitive id.
    /// @param input Input primitive id.
    /// @param mode Type of output - max or min.
    /// @param top_k Number of indices to output.
    /// @param axis Axis to maximize/minimize along.
    /// @param sort Type of sorting - by values or indices.
    arg_max_min(const primitive_id& id,
                const std::vector<input_info>& inputs,
                ov::op::TopKMode mode,
                uint32_t top_k,
                int64_t axis,
                ov::op::TopKSortType sort = ov::op::TopKSortType::SORT_VALUES,
                bool values_first = false,
                const padding& output_padding = padding(),
                data_types output_data_type = data_types::f32,
                const size_t num_outputs = 1)
        : primitive_base(id, inputs, {output_padding}, {optional_data_type{output_data_type}}, num_outputs),
          mode(mode),
          top_k(top_k),
          axis(axis),
          sort(sort),
          values_first(values_first) {}

    /// @brief Constructs arg_max_min for top_k parameter
    arg_max_min(const primitive_id& id,
                const input_info& input,
                const input_info& topk_id,
                ov::op::TopKMode mode,
                uint32_t top_k,
                int64_t axis,
                ov::op::TopKSortType sort = ov::op::TopKSortType::SORT_VALUES,
                bool values_first = false,
                const padding& output_padding = padding(),
                data_types output_data_type = data_types::f32,
                const size_t num_outputs = 1)
        : primitive_base(id, {input, topk_id}, {output_padding}, {optional_data_type{output_data_type}}, num_outputs),
          mode(mode),
          top_k(top_k),
          axis(axis),
          sort(sort),
          values_first(values_first) {}

    /// @brief Type of output - max or min.
    ov::op::TopKMode mode;
    /// @brief Number of indices to output.
    uint32_t top_k;
    /// @brief Axis to maximize/minimize along. If not set, maximize the flattened trailing dimensions for each index of the batch dimension.
    int64_t axis;
    /// @brief Type of sorting - by values or indices.
    ov::op::TopKSortType sort;
    /// @brief Sets output order: if True than first output contains values and second (optional) - indices.
    bool values_first;

    size_t hash() const override {
        size_t seed = primitive::hash();
        seed = hash_combine(seed, mode);
        seed = hash_combine(seed, top_k);
        seed = hash_combine(seed, axis);
        seed = hash_combine(seed, sort);
        seed = hash_combine(seed, values_first);
        return seed;
    }

    bool operator==(const primitive& rhs) const override {
        if (!compare_common_params(rhs))
            return false;

        auto rhs_casted = downcast<const arg_max_min>(rhs);

        return mode == rhs_casted.mode &&
               top_k == rhs_casted.top_k &&
               axis == rhs_casted.axis &&
               sort == rhs_casted.sort &&
               values_first == rhs_casted.values_first;
    }

    size_t get_output_nums() const {
        return (input_size() == 3 ? 2 : output_size());
    }
    bool has_second_output() const { return get_output_nums() == 2; }
    bool use_multiple_outputs() const { return input_size() != 3; }

    void save(BinaryOutputBuffer& ob) const override {
        primitive_base<arg_max_min>::save(ob);
        ob << input;
        ob << num_outputs;
        ob << make_data(&mode, sizeof(ov::op::TopKMode));
        ob << top_k;
        ob << axis;
        ob << make_data(&sort, sizeof(ov::op::TopKSortType));
        ob << values_first;
    }

    void load(BinaryInputBuffer& ib) override {
        primitive_base<arg_max_min>::load(ib);
        ib >> input;
        ib >> num_outputs;
        ib >> make_data(&mode, sizeof(ov::op::TopKMode));
        ib >> top_k;
        ib >> axis;
        ib >> make_data(&sort, sizeof(ov::op::TopKSortType));
        ib >> values_first;
    }
};
}  // namespace cldnn

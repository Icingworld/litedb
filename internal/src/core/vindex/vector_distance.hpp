#pragma once

#include <expected>

#include "core/common/value.hpp"
#include "core/vindex/vector_index.hpp"

namespace litedb::core::vindex
{

/**
 * @brief 计算向量距离
 * @param left 左向量
 * @param right 右向量
 * @param metric 距离度量
 * @return 距离
 */
[[nodiscard]]
std::expected<double, VectorIndexError> vector_distance(
    const common::VectorValue & left,
    const common::VectorValue & right,
    VectorDistanceMetric metric
);

} // namespace litedb::core::vindex

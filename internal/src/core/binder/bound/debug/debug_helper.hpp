#pragma once

#include <string>
#include <string_view>

#include "core/common/logical_type.hpp"
#include "core/common/types.hpp"
#include "core/meta/entry/index_entry.hpp"
#include "core/meta/entry/vector_index_entry.hpp"

namespace litedb::core::binder::bound
{

/**
 * @brief 获取逻辑类型名称
 * @param id 逻辑类型 ID
 * @return 逻辑类型名称
 */
[[nodiscard]]
std::string_view logical_type_name(common::LogicalTypeId id) noexcept;

/**
 * @brief 获取逻辑类型文本
 * @param type 逻辑类型
 * @return 逻辑类型文本
 */
[[nodiscard]]
std::string logical_type_text(const common::LogicalType & type);

/**
 * @brief 获取一元运算符名称
 * @param op 一元运算符
 * @return 一元运算符名称
 */
[[nodiscard]]
std::string_view unary_operator_name(common::UnaryOperator op) noexcept;

/**
 * @brief 获取二元运算符名称
 * @param op 二元运算符
 * @return 二元运算符名称
 */
[[nodiscard]]
std::string_view binary_operator_name(common::BinaryOperator op) noexcept;

/**
 * @brief 获取索引类型名称
 * @param kind 索引类型
 * @return 索引类型名称
 */
[[nodiscard]]
std::string_view index_kind_name(meta::entry::IndexKind kind) noexcept;

/**
 * @brief 获取向量索引类型名称
 * @param kind 向量索引类型
 * @return 向量索引类型名称
 */
[[nodiscard]]
std::string_view vector_index_kind_name(
    meta::entry::VectorIndexKind kind
) noexcept;

/**
 * @brief 获取向量距离度量名称
 * @param metric 向量距离度量
 * @return 向量距离度量名称
 */
[[nodiscard]]
std::string_view vector_distance_metric_name(
    meta::entry::VectorDistanceMetric metric
) noexcept;

} // namespace litedb::core::binder::bound

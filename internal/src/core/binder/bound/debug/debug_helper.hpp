#pragma once

#include <string>
#include <string_view>

#include "core/common/logical_type.hpp"
#include "core/common/types.hpp"
#include "core/meta/entry/index_entry.hpp"
#include "core/meta/entry/vector_index_entry.hpp"

namespace litedb::core::binder::bound
{

// 将逻辑类型 ID 转换为名称
[[nodiscard]]
std::string_view logical_type_name(common::LogicalTypeId id) noexcept;

// 将逻辑类型转换为文本
[[nodiscard]]
std::string logical_type_text(const common::LogicalType & type);

// 将一元运算符转换为名称
[[nodiscard]]
std::string_view unary_operator_name(common::UnaryOperator op) noexcept;

// 将二元运算符转换为名称
[[nodiscard]]
std::string_view binary_operator_name(common::BinaryOperator op) noexcept;

// 将索引类型转换为名称
[[nodiscard]]
std::string_view index_kind_name(meta::entry::IndexKind kind) noexcept;

// 将向量索引类型转换为名称
[[nodiscard]]
std::string_view vector_index_kind_name(meta::entry::VectorIndexKind kind) noexcept;

// 将向量距离度量转换为名称
[[nodiscard]]
std::string_view vector_distance_metric_name(meta::entry::VectorDistanceMetric metric) noexcept;

} // namespace litedb::core::binder::bound

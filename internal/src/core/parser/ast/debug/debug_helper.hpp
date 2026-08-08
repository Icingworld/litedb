#pragma once

#include <string_view>

#include "core/common/logical_type.hpp"
#include "core/parser/ast/statement/create_index_statement.hpp"
#include "core/parser/ast/statement/create_vector_index_statement.hpp"
#include "core/parser/token.hpp"

namespace litedb::core::parser::ast
{

// 将 Token 类型转换为字符串
std::string_view token_type_name(TokenType type) noexcept;

// 将逻辑类型 ID 转换为字符串
std::string_view logical_type_name(common::LogicalTypeId id) noexcept;

// 将创建索引方法转换为字符串
std::string_view create_index_method_name(CreateIndexMethod method) noexcept;

// 将创建向量索引方法转换为字符串
std::string_view create_vector_index_method_name(CreateVectorIndexMethod method) noexcept;

// 将向量索引指标转换为字符串
std::string_view vector_index_metric_name(VectorIndexMetric metric) noexcept;

} // namespace litedb::core::parser::ast

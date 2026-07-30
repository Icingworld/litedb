#pragma once

#include <string_view>

#include "core/common/logical_type.hpp"
#include "core/parser/ast/statement/create_index_statement.hpp"
#include "core/parser/ast/statement/create_vector_index_statement.hpp"
#include "core/parser/token.hpp"

namespace litedb::core::parser::ast
{

/**
 * @brief 获取 Token 类型名称
 * @param type Token 类型
 * @return Token 类型名称
 */
std::string_view token_type_name(TokenType type) noexcept;

/**
 * @brief 获取逻辑类型名称
 * @param id 逻辑类型 ID
 * @return 逻辑类型名称
 */
std::string_view logical_type_name(common::LogicalTypeId id) noexcept;

/**
 * @brief 获取创建索引方法名称
 * @param method 创建索引方法
 * @return 创建索引方法名称
 */
std::string_view create_index_method_name(CreateIndexMethod method) noexcept;

/**
 * @brief 获取创建向量索引方法名称
 * @param method 创建向量索引方法
 * @return 创建向量索引方法名称
 */
std::string_view create_vector_index_method_name(CreateVectorIndexMethod method) noexcept;

/**
 * @brief 获取向量索引指标名称
 * @param metric 向量索引指标
 * @return 向量索引指标名称
 */
std::string_view vector_index_metric_name(VectorIndexMetric metric) noexcept;

} // namespace litedb::core::parser::ast

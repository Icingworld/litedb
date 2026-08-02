#pragma once

#include <cstdint>

#include "core/error/error.hpp"
#include "core/parser/ast/ast_node.hpp"

namespace litedb::core::logical_planner
{

/**
 * @brief 计划错误码
 */
enum class PlannerErrorCode : std::uint8_t
{
    InvalidArgument = 0,                ///< 无效参数
    UnsupportedStatement = 1,           ///< 不支持的语句
};

/**
 * @brief 计划错误
 */
struct PlannerErrorContext
{
    parser::ast::AstNodeLocation location;      ///< 错误位置
};

using PlannerError = error::Error;

} // namespace litedb::core::logical_planner

namespace litedb::core::error
{

template <>
struct ErrorTraits<logical_planner::PlannerErrorCode>
{
    static constexpr ErrorCategory category = ErrorCategory::Planner;
};

} // namespace litedb::core::error

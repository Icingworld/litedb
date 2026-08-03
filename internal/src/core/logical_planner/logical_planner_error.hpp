#pragma once

#include <cstdint>

#include "core/error/error.hpp"
#include "core/parser/ast/ast_node.hpp"

namespace litedb::core::logical_planner
{

/**
 * @brief 逻辑计划器错误码
 */
enum class LogicalPlannerErrorCode : std::uint8_t
{
};

/**
 * @brief 逻辑计划器错误
 */
struct LogicalPlannerErrorContext
{
    parser::ast::AstNodeLocation location;      ///< 错误位置
};

using LogicalPlannerError = error::Error;

} // namespace litedb::core::logical_planner

namespace litedb::core::error
{

template <>
struct ErrorTraits<logical_planner::LogicalPlannerErrorCode>
{
    static constexpr ErrorCategory category = ErrorCategory::LogicalPlanner;
};

} // namespace litedb::core::error

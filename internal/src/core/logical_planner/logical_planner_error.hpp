#pragma once

#include <cstdint>

#include "core/error/error.hpp"
#include "core/parser/ast/ast_node.hpp"

namespace litedb::core::planner
{

/**
 * @brief 计划错误码
 */
enum class PlannerErrorCode : std::uint8_t
{
    InvalidArgument,                ///< 无效参数
    UnsupportedStatement,           ///< 不支持的语句
};

/**
 * @brief 计划错误
 */
struct PlannerErrorContext
{
    parser::ast::AstNodeLocation location;      ///< 错误位置
};

using PlannerError = error::Error;

} // namespace litedb::core::planner

namespace litedb::core::error
{
template <>
struct ErrorTraits<planner::PlannerErrorCode>
{
    static constexpr ErrorCategory category = ErrorCategory::Planner;
};
} // namespace litedb::core::error

#pragma once

#include <cstdint>

#include "core/error/error.hpp"
#include "core/parser/ast/ast_node.hpp"

namespace litedb::core::database
{

/**
 * @brief Session 错误码
 */
enum class SessionErrorCode : std::uint8_t
{
    ParserError,        ///< 解析错误
    BinderError,        ///< 绑定错误
    PlannerError,       ///< 计划错误
    OptimizerError,     ///< 优化错误
    ExecutionError,     ///< 执行错误
};

/**
 * @brief Session 错误
 */
struct SessionErrorContext
{
    parser::ast::AstNodeLocation location;      ///< 错误位置
};

using SessionError = error::Error;

} // namespace litedb::core::database

namespace litedb::core::error
{
template <>
struct ErrorTraits<database::SessionErrorCode>
{
    static constexpr ErrorCategory category = ErrorCategory::Database;
};
} // namespace litedb::core::error

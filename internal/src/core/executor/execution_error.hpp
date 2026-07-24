#pragma once

#include <cstdint>

#include "core/error/error.hpp"
#include "core/parser/ast/ast_node.hpp"

namespace litedb::core::executor
{

/**
 * @brief 执行错误码
 */
enum class ExecutionErrorCode : std::uint8_t
{
    UnsupportedStatement,          ///< 不支持的语句
    InvalidPlan,                   ///< 无效计划
    MetaError,                     ///< 元数据错误
    SchemaError,                   ///< Schema 错误
    StorageError,                  ///< 存储错误
    IndexError,                    ///< 索引错误
    TransactionError,              ///< 事务或 WAL 错误
    EvaluationError,               ///< 表达式求值错误
    CollectionNotFound,            ///< 集合不存在
};

/**
 * @brief 执行错误
 */
struct ExecutionErrorContext
{
    parser::ast::AstNodeLocation location;         ///< 错误位置
};

using ExecutionError = error::Error;

} // namespace litedb::core::executor

namespace litedb::core::error
{
template <>
struct ErrorTraits<executor::ExecutionErrorCode>
{
    static constexpr ErrorCategory category = ErrorCategory::Execution;
};
} // namespace litedb::core::error

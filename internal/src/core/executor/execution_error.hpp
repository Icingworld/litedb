#pragma once

#include <string>

#include "core/parser/ast/ast_node.hpp"

namespace litedb::core::executor
{

/**
 * @brief 执行错误码
 */
enum class ExecutionErrorCode
{
    UnsupportedStatement,          ///< 不支持的语句
    InvalidPlan,                   ///< 无效计划
    MetaError,                     ///< 元数据错误
    SchemaError,                   ///< Schema 错误
    StorageError,                  ///< 存储错误
    IndexError,                    ///< 索引错误
    EvaluationError,               ///< 表达式求值错误
    CollectionNotFound,            ///< 集合不存在
};

/**
 * @brief 执行错误
 */
struct ExecutionError
{
    ExecutionErrorCode code;                       ///< 错误码
    parser::ast::AstNodeLocation location;         ///< 错误位置
    std::string message;                           ///< 错误消息
};

} // namespace litedb::core::executor

#pragma once

#include <string>

#include "core/parser/ast/ast_node.hpp"

namespace litedb::core::engine
{

/**
 * @brief Engine 错误码
 */
enum class EngineErrorCode
{
    ParserError,        ///< 解析错误
    BinderError,        ///< 绑定错误
    PlannerError,       ///< 计划错误
    ExecutionError,     ///< 执行错误
};

/**
 * @brief Engine 错误
 */
struct EngineError
{
    EngineErrorCode code;                       ///< 错误码
    parser::ast::AstNodeLocation location;      ///< 错误位置
    std::string message;                        ///< 错误消息
};

} // namespace litedb::core::engine

#pragma once

#include <string>

#include "core/parser/ast/ast_node.hpp"

namespace litedb::core::function
{

/**
 * @brief 函数错误代码
 */
enum class FunctionErrorCode
{
    InvalidArgument,        ///< 无效参数
    InvalidType,            ///< 无效类型
};

/**
 * @brief 函数错误
 */
struct FunctionError
{
    FunctionErrorCode code;                     ///< 错误代码
    parser::ast::AstNodeLocation location;      ///< 位置
    std::string message;                        ///< 消息
};

} // namespace litedb::core::function

#pragma once

#include <string>

#include "core/parser/ast/ast_node.hpp"

namespace litedb::core::optimizer
{

/**
 * @brief 优化器错误码
 */
enum class OptimizerErrorCode
{
    InvalidArgument,    ///< 无效参数
};

/**
 * @brief 优化器错误
 */
struct OptimizerError
{
    OptimizerErrorCode code;                    ///< 错误码
    parser::ast::AstNodeLocation location;      ///< 错误位置
    std::string message;                        ///< 错误消息
};

} // namespace litedb::core::optimizer

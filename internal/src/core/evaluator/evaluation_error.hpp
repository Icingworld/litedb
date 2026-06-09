#pragma once

#include <string>

#include "core/parser/ast/ast_node.hpp"

namespace litedb::core::evaluator
{

/**
 * @brief 评估错误代码
 */
enum class EvaluationErrorCode
{
    UnsupportedExpression,                      ///< 不支持的表达式
    InvalidType,                                ///< 无效的类型
    InvalidLiteral,                             ///< 无效的字面量
    InvalidColumnReference,                     ///< 无效的列引用
    DivisionByZero,                             ///< 除以零
    CastFailed                                  ///< 转换失败
};

/**
 * @brief 评估错误
 */
struct EvaluationError
{
    EvaluationErrorCode code;                   ///< 评估错误代码
    parser::ast::AstNodeLocation location;      ///< 评估错误位置
    std::string message;                        ///< 评估错误消息
};

} // namespace litedb::core::evaluator

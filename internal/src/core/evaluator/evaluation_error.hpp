#pragma once

#include <cstdint>

#include "core/error/error.hpp"
#include "core/parser/ast/ast_node.hpp"

namespace litedb::core::evaluator
{

/**
 * @brief 评估错误代码
 */
enum class EvaluationErrorCode : std::uint8_t
{
    UnsupportedExpression,                      ///< 不支持的表达式
    InvalidType,                                ///< 无效的类型
    InvalidLiteral,                             ///< 无效的字面量
    InvalidColumnReference,                     ///< 无效的列引用
    DivisionByZero,                             ///< 除以零
    CastFailed,                                 ///< 转换失败
};

/**
 * @brief 评估错误
 */
struct EvaluationErrorContext
{
    parser::ast::AstNodeLocation location;      ///< 评估错误位置
};

using EvaluationError = error::Error;

} // namespace litedb::core::evaluator

namespace litedb::core::error
{
template <>
struct ErrorTraits<evaluator::EvaluationErrorCode>
{
    static constexpr ErrorCategory category = ErrorCategory::Evaluation;
};
} // namespace litedb::core::error

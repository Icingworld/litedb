#pragma once

#include <cstdint>

#include "core/error/error.hpp"

namespace litedb::core::evaluator
{

// 评估错误代码
enum class EvaluationErrorCode : std::uint8_t
{
    UnsupportedExpression, // 不支持的表达式
    InvalidType, // 无效的类型
    InvalidColumnReference, // 无效的列引用
    DivisionByZero, // 除以零
    CastFailed, // 转换失败
    NumericOverflow, // 数值溢出
    FunctionError, // 标量函数求值失败
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

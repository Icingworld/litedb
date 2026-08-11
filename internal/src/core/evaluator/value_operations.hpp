#pragma once

#include <expected>

#include "core/common/logical_type.hpp"
#include "core/common/types.hpp"
#include "core/common/value.hpp"
#include "core/evaluator/evaluation_error.hpp"

namespace litedb::core::evaluator
{

// 评估一元操作数
[[nodiscard]]
std::expected<common::Value, EvaluationError> evaluate_unary_value(
    common::UnaryOperator op,
    const common::Value & operand,
    const common::LogicalType & result_type
);

// 评估二元操作数
[[nodiscard]]
std::expected<common::Value, EvaluationError> evaluate_binary_values(
    common::BinaryOperator op,
    const common::Value & left,
    const common::Value & right,
    const common::LogicalType & result_type
);

// 比较两个值
[[nodiscard]]
std::expected<common::Value, EvaluationError>
compare_values(const common::Value & left, common::BinaryOperator op, const common::Value & right);

// 逻辑与
[[nodiscard]]
std::expected<common::Value, EvaluationError>
logical_and(const common::Value & left, const common::Value & right);

// 逻辑或
[[nodiscard]]
std::expected<common::Value, EvaluationError>
logical_or(const common::Value & left, const common::Value & right);

// 转换值
[[nodiscard]]
std::expected<common::Value, EvaluationError>
cast_value(const common::Value & value, const common::LogicalType & target_type);

// 评估 like 值
[[nodiscard]]
std::expected<common::Value, EvaluationError>
evaluate_like_values(const common::Value & value, const common::Value & pattern);

// 评估当前行是否需要保留
[[nodiscard]]
std::expected<bool, EvaluationError> filter_matches(const common::Value & value);

} // namespace litedb::core::evaluator

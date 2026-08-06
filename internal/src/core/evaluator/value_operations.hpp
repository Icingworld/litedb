#pragma once

#include <expected>

#include "core/common/logical_type.hpp"
#include "core/common/types.hpp"
#include "core/common/value.hpp"
#include "core/evaluator/evaluation_error.hpp"

namespace litedb::core::evaluator
{

/**
 * @brief 评估一元操作数
 * @param op 操作符
 * @param operand 操作数
 * @param result_type 结果类型
 * @return 结果
 */
[[nodiscard]]
std::expected<common::Value, EvaluationError> evaluate_unary_value(
    common::UnaryOperator op,
    const common::Value & operand,
    const common::LogicalType & result_type
);

/**
 * @brief 评估二元操作数
 * @param op 操作符
 * @param left 左操作数
 * @param right 右操作数
 * @param result_type 结果类型
 * @return 结果
 */
[[nodiscard]]
std::expected<common::Value, EvaluationError> evaluate_binary_values(
    common::BinaryOperator op,
    const common::Value & left,
    const common::Value & right,
    const common::LogicalType & result_type
);

/**
 * @brief 比较两个值
 * @param left 左操作数
 * @param op 操作符
 * @param right 右操作数
 * @return 结果
 */
[[nodiscard]]
std::expected<common::Value, EvaluationError> compare_values(
    const common::Value & left,
    common::BinaryOperator op,
    const common::Value & right
);

/**
 * @brief 逻辑与
 * @param left 左操作数
 * @param right 右操作数
 * @return 结果
 */
[[nodiscard]]
std::expected<common::Value, EvaluationError> logical_and(
    const common::Value & left,
    const common::Value & right
);

/**
 * @brief 逻辑或
 * @param left 左操作数
 * @param right 右操作数
 * @return 结果
 */
[[nodiscard]]
std::expected<common::Value, EvaluationError> logical_or(
    const common::Value & left,
    const common::Value & right
);

/**
 * @brief 转换值
 * @param value 值
 * @param target_type 目标类型
 * @return 结果
 */
[[nodiscard]]
std::expected<common::Value, EvaluationError> cast_value(
    const common::Value & value,
    const common::LogicalType & target_type
);

/**
 * @brief 评估 like 值
 * @param value 值
 * @param pattern 模式
 * @return 结果
 */
[[nodiscard]]
std::expected<common::Value, EvaluationError> evaluate_like_values(
    const common::Value & value,
    const common::Value & pattern
);

/**
 * @brief 评估当前行是否需要保留
 * @param value 值
 * @return 是否需要保留
 */
[[nodiscard]]
std::expected<bool, EvaluationError> filter_matches(
    const common::Value & value
);

} // namespace litedb::core::evaluator

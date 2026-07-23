#pragma once

#include <expected>

#include "core/binder/bound/expression/bound_expression.hpp"
#include "core/evaluator/evaluation_error.hpp"
#include "core/common/record.hpp"
#include "core/common/value.hpp"

namespace litedb::core::evaluator
{

/**
 * @brief 表达式评估器
 */
class ExpressionEvaluator
{
public:
    /**
     * @brief 评估表达式
     * @param expression 表达式
     * @param record 记录
     * @return 评估结果
     */
    [[nodiscard]]
    std::expected<common::Value, EvaluationError> evaluate(
        const binder::bound::BoundExpression & expression,
        const common::Record & record
    ) const;

    /**
     * @brief 评估谓词表达式
     * @param expression 表达式
     * @param record 记录
     * @return 评估结果
     */
    [[nodiscard]]
    std::expected<bool, EvaluationError> evaluate_predicate(
        const binder::bound::BoundExpression & expression,
        const common::Record & record
    ) const;
};

} // namespace litedb::core::evaluator

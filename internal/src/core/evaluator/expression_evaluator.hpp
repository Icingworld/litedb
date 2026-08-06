#pragma once

#include <expected>

#include "core/binder/bound/dispatcher/expression_dispatcher.hpp"
#include "core/evaluator/evaluation_context.hpp"
#include "core/evaluator/evaluation_error.hpp"

namespace litedb::core::evaluator
{

/**
 * @brief 表达式评估器
 */
class ExpressionEvaluator final
    : private binder::bound::ConstBoundExpressionDispatcher<
          ExpressionEvaluator,
          std::expected<common::Value, EvaluationError>
      >
{
    friend binder::bound::ConstBoundExpressionDispatcher<
        ExpressionEvaluator,
        std::expected<common::Value, EvaluationError>
    >;

public:
    explicit ExpressionEvaluator(EvaluationContext context = {});

public:
    /**
     * @brief 评估表达式
     * @param expression 表达式
     * @return 值
     */
    [[nodiscard]]
    std::expected<common::Value, EvaluationError> evaluate(
        const binder::bound::BoundExpression & expression
    );

    /**
     * @brief 评估当前行是否需要保留
     * @param expression 表达式
     * @return 是否需要保留
     */
    [[nodiscard]]
    std::expected<bool, EvaluationError> evaluate_filter(
        const binder::bound::BoundExpression & expression
    );

    /**
     * @brief 评估常量表达式
     * @param expression 表达式
     * @param function_context 函数上下文
     * @return 值
     */
    [[nodiscard]]
    static std::expected<common::Value, EvaluationError> evaluate_constant(
        const binder::bound::BoundExpression & expression,
        function::ScalarFunctionContext function_context = {}
    );

private:
    /**
     * @brief 访问字面量表达式
     * @param expression 表达式
     * @return 值
     */
    std::expected<common::Value, EvaluationError> visit_literal_expression(
        const binder::bound::BoundLiteralExpression & expression
    );

    /**
     * @brief 访问空表达式
     * @param expression 表达式
     * @return 值
     */
    std::expected<common::Value, EvaluationError> visit_null_expression(
        const binder::bound::BoundNullExpression & expression
    );

    /**
     * @brief 访问列引用表达式
     * @param expression 表达式
     * @return 值
     */
    std::expected<common::Value, EvaluationError> visit_column_ref_expression(
        const binder::bound::BoundColumnRefExpression & expression
    );

    /**
     * @brief 访问一元表达式
     * @param expression 表达式
     * @return 值
     */
    std::expected<common::Value, EvaluationError> visit_unary_expression(
        const binder::bound::BoundUnaryExpression & expression
    );

    /**
     * @brief 访问二元表达式
     * @param expression 表达式
     * @return 值
     */
    std::expected<common::Value, EvaluationError> visit_binary_expression(
        const binder::bound::BoundBinaryExpression & expression
    );

    /**
     * @brief 访问向量表达式
     * @param expression 表达式
     * @return 值
     */
    std::expected<common::Value, EvaluationError> visit_vector_expression(
        const binder::bound::BoundVectorExpression & expression
    );

    /**
     * @brief 访问函数表达式
     * @param expression 表达式
     * @return 值
     */
    std::expected<common::Value, EvaluationError> visit_function_expression(
        const binder::bound::BoundFunctionExpression & expression
    );

    /**
     * @brief 访问in表达式
     * @param expression 表达式
     * @return 值
     */
    std::expected<common::Value, EvaluationError> visit_in_expression(
        const binder::bound::BoundInExpression & expression
    );

    /**
     * @brief 访问between表达式
     * @param expression 表达式
     * @return 值
     */
    std::expected<common::Value, EvaluationError> visit_between_expression(
        const binder::bound::BoundBetweenExpression & expression
    );

    /**
     * @brief 访问like表达式
     * @param expression 表达式
     * @return 值
     */
    std::expected<common::Value, EvaluationError> visit_like_expression(
        const binder::bound::BoundLikeExpression & expression
    );

    /**
     * @brief 访问cast表达式
     * @param expression 表达式
     * @return 值
     */
    std::expected<common::Value, EvaluationError> visit_cast_expression(
        const binder::bound::BoundCastExpression & expression
    );

private:
    EvaluationContext context_;     ///< 评估上下文
};

} // namespace litedb::core::evaluator

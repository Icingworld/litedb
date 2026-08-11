#pragma once

#include <expected>

#include "core/binder/bound/dispatcher/expression_dispatcher.hpp"
#include "core/evaluator/evaluation_context.hpp"
#include "core/evaluator/evaluation_error.hpp"

namespace litedb::core::evaluator
{

// 表达式评估器
class ExpressionEvaluator final
    : private binder::bound::ConstBoundExpressionDispatcher<
          ExpressionEvaluator,
          std::expected<common::Value, EvaluationError>>
{
    friend binder::bound::ConstBoundExpressionDispatcher<
        ExpressionEvaluator,
        std::expected<common::Value, EvaluationError>>;

public:
    explicit ExpressionEvaluator(EvaluationContext context = {});

public:
    // 评估表达式
    [[nodiscard]]
    std::expected<common::Value, EvaluationError> evaluate(
        const binder::bound::BoundExpression & expression
    );

    // 评估当前行是否需要保留
    [[nodiscard]]
    std::expected<bool, EvaluationError> evaluate_filter(
        const binder::bound::BoundExpression & expression
    );

    // 评估常量表达式
    [[nodiscard]]
    static std::expected<common::Value, EvaluationError> evaluate_constant(
        const binder::bound::BoundExpression & expression,
        function::ScalarFunctionContext function_context = {}
    );

private:
    // 访问字面量表达式
    std::expected<common::Value, EvaluationError> visit_literal_expression(
        const binder::bound::BoundLiteralExpression & expression
    );

    // 访问空表达式
    std::expected<common::Value, EvaluationError> visit_null_expression(
        const binder::bound::BoundNullExpression & expression
    );

    // 访问列引用表达式
    std::expected<common::Value, EvaluationError> visit_column_ref_expression(
        const binder::bound::BoundColumnRefExpression & expression
    );

    // 访问一元表达式
    std::expected<common::Value, EvaluationError> visit_unary_expression(
        const binder::bound::BoundUnaryExpression & expression
    );

    // 访问二元表达式
    std::expected<common::Value, EvaluationError> visit_binary_expression(
        const binder::bound::BoundBinaryExpression & expression
    );

    // 访问向量表达式
    std::expected<common::Value, EvaluationError> visit_vector_expression(
        const binder::bound::BoundVectorExpression & expression
    );

    // 访问函数表达式
    std::expected<common::Value, EvaluationError> visit_function_expression(
        const binder::bound::BoundFunctionExpression & expression
    );

    // 访问 IN 表达式
    std::expected<common::Value, EvaluationError> visit_in_expression(
        const binder::bound::BoundInExpression & expression
    );

    // 访问 BETWEEN 表达式
    std::expected<common::Value, EvaluationError> visit_between_expression(
        const binder::bound::BoundBetweenExpression & expression
    );

    // 访问 LIKE 表达式
    std::expected<common::Value, EvaluationError> visit_like_expression(
        const binder::bound::BoundLikeExpression & expression
    );

    // 访问 CAST 表达式
    std::expected<common::Value, EvaluationError> visit_cast_expression(
        const binder::bound::BoundCastExpression & expression
    );

private:
    EvaluationContext context_;
};

} // namespace litedb::core::evaluator

#include "core/evaluator/expression_evaluator.hpp"

#include <string>
#include <utility>
#include <vector>

#include "core/common/types.hpp"
#include "core/evaluator/evaluator_helper.hpp"
#include "core/evaluator/value_operations.hpp"

namespace litedb::core::evaluator
{

ExpressionEvaluator::ExpressionEvaluator(EvaluationContext context)
    : context_(std::move(context))
{}

std::expected<common::Value, EvaluationError> ExpressionEvaluator::evaluate(
    const binder::bound::BoundExpression & expression
)
{
    return dispatch_expression(expression);
}

std::expected<bool, EvaluationError> ExpressionEvaluator::evaluate_filter(
    const binder::bound::BoundExpression & expression
)
{
    auto value = evaluate(expression);
    if (!value.has_value()) {
        return std::unexpected(std::move(value.error()));
    }
    return filter_matches(*value);
}

std::expected<common::Value, EvaluationError> ExpressionEvaluator::evaluate_constant(
    const binder::bound::BoundExpression & expression,
    function::ScalarFunctionContext function_context
)
{
    ExpressionEvaluator evaluator {EvaluationContext {
        .input_values = {},
        .function_context = std::move(function_context),
    }};
    return evaluator.evaluate(expression);
}

std::expected<common::Value, EvaluationError> ExpressionEvaluator::visit_literal_expression(
    const binder::bound::BoundLiteralExpression & expression
)
{
    if (!expression.value().matches_type(expression.type())) {
        return std::unexpected(make_error(
            EvaluationErrorCode::InvalidType,
            "Bound literal value does not match its logical type"
        ));
    }
    return expression.value();
}

std::expected<common::Value, EvaluationError> ExpressionEvaluator::visit_null_expression(
    const binder::bound::BoundNullExpression &
)
{
    return common::Value::null();
}

std::expected<common::Value, EvaluationError> ExpressionEvaluator::visit_column_ref_expression(
    const binder::bound::BoundColumnRefExpression & expression
)
{
    if (expression.column_ordinal() >= context_.input_values.size()) {
        return std::unexpected(make_error(
            EvaluationErrorCode::InvalidColumnReference,
            "Column ordinal is outside the evaluation input"
        ));
    }

    const auto & value = context_.input_values[expression.column_ordinal()];
    if (!value.matches_type(expression.type())) {
        return std::unexpected(make_error(
            EvaluationErrorCode::InvalidType,
            "Column value does not match its bound logical type"
        ));
    }
    return value;
}

std::expected<common::Value, EvaluationError> ExpressionEvaluator::visit_unary_expression(
    const binder::bound::BoundUnaryExpression & expression
)
{
    // 评估操作数
    auto operand = evaluate(expression.operand());
    if (!operand.has_value()) {
        return std::unexpected(std::move(operand.error()));
    }
    return evaluate_unary_value(expression.op(), *operand, expression.type());
}

std::expected<common::Value, EvaluationError> ExpressionEvaluator::visit_binary_expression(
    const binder::bound::BoundBinaryExpression & expression
)
{
    // 评估左操作数
    auto left = evaluate(expression.left());
    if (!left.has_value()) {
        return std::unexpected(std::move(left.error()));
    }

    // 表达式短路求值
    if (expression.op() == common::BinaryOperator::And) {
        if (const auto * boolean = std::get_if<bool>(&left->data());
            boolean != nullptr && !*boolean) {
            // 左操作数为 false 且操作类型为 AND，直接返回false
            return common::Value {common::ValueData {false}};
        }
    } else if (expression.op() == common::BinaryOperator::Or) {
        if (const auto * boolean = std::get_if<bool>(&left->data());
            boolean != nullptr && *boolean) {
            // 左操作数为 true 且操作类型为 OR，直接返回true
            return common::Value {common::ValueData {true}};
        }
    }

    // 评估右操作数
    auto right = evaluate(expression.right());
    if (!right.has_value()) {
        return std::unexpected(std::move(right.error()));
    }
    return evaluate_binary_values(expression.op(), *left, *right, expression.type());
}

std::expected<common::Value, EvaluationError> ExpressionEvaluator::visit_vector_expression(
    const binder::bound::BoundVectorExpression & expression
)
{
    common::VectorValue elements;
    elements.reserve(expression.elements().size());
    for (const auto & element : expression.elements()) {
        auto value = evaluate(*element);
        if (!value.has_value()) {
            return std::unexpected(std::move(value.error()));
        }
        if (value->is_null()) {
            return common::Value::null();
        }

        // 向量元素统一使用 double 类型
        const auto * number = std::get_if<double>(&value->data());
        if (number == nullptr) {
            return std::unexpected(make_error(
                EvaluationErrorCode::InvalidType,
                "Bound vector elements must evaluate to DOUBLE"
            ));
        }
        elements.push_back(*number);
    }
    return common::Value {common::ValueData {std::move(elements)}};
}

std::expected<common::Value, EvaluationError> ExpressionEvaluator::visit_function_expression(
    const binder::bound::BoundFunctionExpression & expression
)
{
    // 逐个评估函数参数
    std::vector<common::Value> arguments;
    arguments.reserve(expression.arguments().size());
    for (const auto & argument : expression.arguments()) {
        auto value = evaluate(*argument);
        if (!value.has_value()) {
            return std::unexpected(std::move(value.error()));
        }
        arguments.push_back(std::move(*value));
    }

    // 调用函数的 evaluate 接口计算结果
    auto value = expression.function().evaluate(arguments, context_.function_context);
    if (!value.has_value()) {
        return std::unexpected(std::move(value.error()));
    }
    if (!value->matches_type(expression.type())) {
        return std::unexpected(make_error(
            EvaluationErrorCode::InvalidType,
            "Scalar function result does not match its bound return type"
        ));
    }
    return std::move(*value);
}

std::expected<common::Value, EvaluationError> ExpressionEvaluator::visit_in_expression(
    const binder::bound::BoundInExpression & expression
)
{
    // 评估目标值
    auto target = evaluate(expression.expression());
    if (!target.has_value()) {
        return std::unexpected(std::move(target.error()));
    }
    if (target->is_null()) {
        return common::Value::null();
    }

    // 评估候选值列表
    auto saw_null = false;
    for (const auto & candidate_expression : expression.values()) {
        // 评估候选值
        auto candidate = evaluate(*candidate_expression);
        if (!candidate.has_value()) {
            return std::unexpected(std::move(candidate.error()));
        }
        if (candidate->is_null()) {
            // 候选值为 null，记下，但不能判定与它相等
            // 继续评估其他候选值，如果遍历结束依然未命中，则返回 null
            saw_null = true;
            continue;
        }

        auto equal = compare_values(*target, common::BinaryOperator::Equal, *candidate);
        if (!equal.has_value()) {
            return std::unexpected(std::move(equal.error()));
        }
        if (std::get<bool>(equal->data())) {
            return common::Value {common::ValueData {true}};
        }
    }

    if (saw_null) {
        // 遇到过 null 值，遍历结束依然未命中，返回 null
        return common::Value::null();
    }
    // 没有遇到过 null 值，遍历结束依然未命中，返回 false
    return common::Value {common::ValueData {false}};
}

std::expected<common::Value, EvaluationError> ExpressionEvaluator::visit_between_expression(
    const binder::bound::BoundBetweenExpression & expression
)
{
    // 评估目标值
    auto target = evaluate(expression.expression());
    if (!target.has_value()) {
        return std::unexpected(std::move(target.error()));
    }

    // 评估下界值
    auto lower = evaluate(expression.lower());
    if (!lower.has_value()) {
        return std::unexpected(std::move(lower.error()));
    }

    // 评估上界值
    auto upper = evaluate(expression.upper());
    if (!upper.has_value()) {
        return std::unexpected(std::move(upper.error()));
    }

    auto lower_result = compare_values(*target, common::BinaryOperator::GreaterThanOrEqual, *lower);
    if (!lower_result.has_value()) {
        return std::unexpected(std::move(lower_result.error()));
    }

    auto upper_result = compare_values(*target, common::BinaryOperator::LessThanOrEqual, *upper);
    if (!upper_result.has_value()) {
        return std::unexpected(std::move(upper_result.error()));
    }
    return logical_and(*lower_result, *upper_result);
}

std::expected<common::Value, EvaluationError> ExpressionEvaluator::visit_like_expression(
    const binder::bound::BoundLikeExpression & expression
)
{
    // 评估目标值
    auto value = evaluate(expression.expression());
    if (!value.has_value()) {
        return std::unexpected(std::move(value.error()));
    }

    // 评估模式值
    auto pattern = evaluate(expression.pattern());
    if (!pattern.has_value()) {
        return std::unexpected(std::move(pattern.error()));
    }
    return evaluate_like_values(*value, *pattern);
}

std::expected<common::Value, EvaluationError> ExpressionEvaluator::visit_cast_expression(
    const binder::bound::BoundCastExpression & expression
)
{
    // 评估操作数
    auto value = evaluate(expression.expression());
    if (!value.has_value()) {
        return std::unexpected(std::move(value.error()));
    }
    return cast_value(*value, expression.type());
}

} // namespace litedb::core::evaluator

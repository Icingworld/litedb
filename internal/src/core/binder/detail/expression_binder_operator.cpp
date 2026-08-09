#include "core/binder/detail/expression_binder.hpp"

#include <utility>

#include "core/binder/binder_helper.hpp"
#include "core/binder/bound/expression/bound_binary_expression.hpp"
#include "core/binder/bound/expression/bound_unary_expression.hpp"

namespace litedb::core::binder::detail
{

using namespace litedb::core::binder::bound;
using namespace litedb::core::common;
using namespace litedb::core::parser;
using namespace litedb::core::parser::ast;

namespace
{

common::BinaryOperator binary_operator(TokenType op)
{
    switch (op) {
    case TokenType::Plus:
        return common::BinaryOperator::Add;
    case TokenType::Minus:
        return common::BinaryOperator::Subtract;
    case TokenType::Star:
        return common::BinaryOperator::Multiply;
    case TokenType::Slash:
        return common::BinaryOperator::Divide;
    case TokenType::Modulo:
        return common::BinaryOperator::Modulus;
    case TokenType::Equal:
        return common::BinaryOperator::Equal;
    case TokenType::NotEqual:
        return common::BinaryOperator::NotEqual;
    case TokenType::LessThan:
        return common::BinaryOperator::LessThan;
    case TokenType::LessEqual:
        return common::BinaryOperator::LessThanOrEqual;
    case TokenType::GreaterThan:
        return common::BinaryOperator::GreaterThan;
    case TokenType::GreaterEqual:
        return common::BinaryOperator::GreaterThanOrEqual;
    case TokenType::And:
        return common::BinaryOperator::And;
    case TokenType::Or:
        return common::BinaryOperator::Or;
    default:
        std::unreachable();
    }
}

} // namespace

std::expected<std::unique_ptr<bound::BoundExpression>, BinderError>
ExpressionBinder::visit_unary_expression(const UnaryExpression & expression)
{
    auto operand = bind(expression.operand());
    if (!operand.has_value()) [[unlikely]] {
        return std::unexpected(std::move(operand.error()));
    }

    if (expression.op() == TokenType::Not) {
        if (!is_boolean((*operand)->type())) [[unlikely]] {
            return std::unexpected(make_binder_error(
                BinderErrorCode::InvalidType,
                expression.location(),
                "NOT operand must be BOOLEAN"
            ));
        }
        return std::make_unique<BoundUnaryExpression>(
            common::UnaryOperator::Not,
            std::move(*operand),
            type(LogicalTypeId::Boolean)
        );
    }

    if ((expression.op() == TokenType::Plus || expression.op() == TokenType::Minus) &&
        is_numeric((*operand)->type())) {
        if (expression.op() == TokenType::Plus) {
            return std::move(*operand);
        }
        const auto result_type = (*operand)->type();
        return std::make_unique<BoundUnaryExpression>(
            common::UnaryOperator::Negate,
            std::move(*operand),
            result_type
        );
    }

    [[unlikely]] return std::unexpected(make_binder_error(
        BinderErrorCode::InvalidType,
        expression.location(),
        "Invalid unary operand type"
    ));
}

std::expected<std::unique_ptr<bound::BoundExpression>, BinderError>
ExpressionBinder::visit_binary_expression(const BinaryExpression & expression)
{
    auto left = bind(expression.left());
    if (!left.has_value()) [[unlikely]] {
        return std::unexpected(std::move(left.error()));
    }

    auto right = bind(expression.right());
    if (!right.has_value()) [[unlikely]] {
        return std::unexpected(std::move(right.error()));
    }

    const auto token_op = expression.op();
    if (token_op == TokenType::And || token_op == TokenType::Or) {
        if (!is_boolean((*left)->type()) || !is_boolean((*right)->type())) [[unlikely]] {
            return std::unexpected(make_binder_error(
                BinderErrorCode::InvalidType,
                expression.location(),
                "Logical operands must be BOOLEAN"
            ));
        }

        return std::make_unique<BoundBinaryExpression>(
            std::move(*left),
            binary_operator(token_op),
            std::move(*right),
            type(LogicalTypeId::Boolean)
        );
    }

    if (token_op == TokenType::Plus || token_op == TokenType::Minus ||
        token_op == TokenType::Star || token_op == TokenType::Slash ||
        token_op == TokenType::Modulo) {
        if (!is_numeric((*left)->type()) || !is_numeric((*right)->type())) [[unlikely]] {
            return std::unexpected(make_binder_error(
                BinderErrorCode::InvalidType,
                expression.location(),
                "Arithmetic operands must be numeric"
            ));
        }
        const auto result_type = common_numeric_type((*left)->type(), (*right)->type());
        return std::make_unique<BoundBinaryExpression>(
            cast_if_needed(std::move(*left), result_type),
            binary_operator(token_op),
            cast_if_needed(std::move(*right), result_type),
            result_type
        );
    }

    if (token_op == TokenType::Equal || token_op == TokenType::NotEqual ||
        token_op == TokenType::LessThan || token_op == TokenType::LessEqual ||
        token_op == TokenType::GreaterThan || token_op == TokenType::GreaterEqual) {
        const auto op = binary_operator(token_op);
        if (!can_compare((*left)->type(), (*right)->type(), op)) [[unlikely]] {
            return std::unexpected(make_binder_error(
                BinderErrorCode::InvalidType,
                expression.location(),
                "Comparison operands are not compatible"
            ));
        }
        if (is_numeric((*left)->type()) && is_numeric((*right)->type())) {
            const auto common_type = common_numeric_type((*left)->type(), (*right)->type());
            left = cast_if_needed(std::move(*left), common_type);
            right = cast_if_needed(std::move(*right), common_type);
        }
        return std::make_unique<BoundBinaryExpression>(
            std::move(*left),
            op,
            std::move(*right),
            type(LogicalTypeId::Boolean)
        );
    }

    [[unlikely]] return std::unexpected(make_binder_error(
        BinderErrorCode::UnsupportedExpression,
        expression.location(),
        "Unsupported binary operator"
    ));
}

} // namespace litedb::core::binder::detail

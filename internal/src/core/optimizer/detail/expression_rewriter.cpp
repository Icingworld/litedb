#include "core/optimizer/detail/expression_rewriter.hpp"

#include <cassert>
#include <memory>
#include <optional>
#include <utility>
#include <variant>

#include "core/binder/bound/expression/bound_between_expression.hpp"
#include "core/binder/bound/expression/bound_binary_expression.hpp"
#include "core/binder/bound/expression/bound_cast_expression.hpp"
#include "core/binder/bound/expression/bound_expression.hpp"
#include "core/binder/bound/expression/bound_function_expression.hpp"
#include "core/binder/bound/expression/bound_in_expression.hpp"
#include "core/binder/bound/expression/bound_like_expression.hpp"
#include "core/binder/bound/expression/bound_literal_expression.hpp"
#include "core/binder/bound/expression/bound_null_expression.hpp"
#include "core/binder/bound/expression/bound_unary_expression.hpp"
#include "core/binder/bound/expression/bound_vector_expression.hpp"
#include "core/common/types.hpp"
#include "core/common/value.hpp"
#include "core/evaluator/expression_evaluator.hpp"

namespace litedb::core::optimizer::detail
{

namespace
{

using binder::bound::BoundBetweenExpression;
using binder::bound::BoundBinaryExpression;
using binder::bound::BoundCastExpression;
using binder::bound::BoundExpression;
using binder::bound::BoundExpressionKind;
using binder::bound::BoundFunctionExpression;
using binder::bound::BoundInExpression;
using binder::bound::BoundLikeExpression;
using binder::bound::BoundLiteralExpression;
using binder::bound::BoundNullExpression;
using binder::bound::BoundUnaryExpression;
using binder::bound::BoundVectorExpression;

template <typename ExpressionType>
[[nodiscard]]
std::unique_ptr<ExpressionType> owning_downcast(
    std::unique_ptr<BoundExpression> expression,
    BoundExpressionKind expected_kind
) noexcept
{
    assert(expression != nullptr);
    assert(expression->kind() == expected_kind);
    return std::unique_ptr<ExpressionType>(static_cast<ExpressionType *>(expression.release()));
}

struct RewriteResult
{
    std::unique_ptr<BoundExpression> expression;
    bool constant_foldable {false};
};

[[nodiscard]]
std::unique_ptr<BoundExpression> make_boolean_literal(bool value)
{
    return std::make_unique<BoundLiteralExpression>(
        common::LogicalType {common::LogicalTypeId::Boolean, std::nullopt},
        common::Value {common::ValueData {value}}
    );
}

[[nodiscard]]
RewriteResult try_fold_constant(RewriteResult result)
{
    assert(result.expression != nullptr);
    if (!result.constant_foldable) {
        return result;
    }

    auto value = evaluator::ExpressionEvaluator::evaluate_constant(*result.expression);
    if (!value.has_value()) {
        return result;
    }

    const auto type = result.expression->type();
    if (value->is_null()) {
        result.expression = std::make_unique<BoundNullExpression>(type);
    } else {
        result.expression = std::make_unique<BoundLiteralExpression>(type, std::move(*value));
    }
    result.constant_foldable = true;
    return result;
}

class ExpressionRewriter final
{
public:
    [[nodiscard]]
    RewriteResult rewrite(std::unique_ptr<BoundExpression> expression)
    {
        assert(expression != nullptr);
        switch (expression->kind()) {
        case BoundExpressionKind::Literal:
        case BoundExpressionKind::Null:
            return RewriteResult {
                .expression = std::move(expression),
                .constant_foldable = true,
            };
        case BoundExpressionKind::ColumnRef:
            return RewriteResult {
                .expression = std::move(expression),
                .constant_foldable = false,
            };
        case BoundExpressionKind::Unary:
            return rewrite_unary(
                owning_downcast<BoundUnaryExpression>(
                    std::move(expression),
                    BoundExpressionKind::Unary
                )
            );
        case BoundExpressionKind::Binary:
            return rewrite_binary(
                owning_downcast<BoundBinaryExpression>(
                    std::move(expression),
                    BoundExpressionKind::Binary
                )
            );
        case BoundExpressionKind::Vector:
            return rewrite_vector(
                owning_downcast<BoundVectorExpression>(
                    std::move(expression),
                    BoundExpressionKind::Vector
                )
            );
        case BoundExpressionKind::Function:
            return rewrite_function(
                owning_downcast<BoundFunctionExpression>(
                    std::move(expression),
                    BoundExpressionKind::Function
                )
            );
        case BoundExpressionKind::In:
            return rewrite_in(
                owning_downcast<BoundInExpression>(std::move(expression), BoundExpressionKind::In)
            );
        case BoundExpressionKind::Between:
            return rewrite_between(
                owning_downcast<BoundBetweenExpression>(
                    std::move(expression),
                    BoundExpressionKind::Between
                )
            );
        case BoundExpressionKind::Like:
            return rewrite_like(
                owning_downcast<BoundLikeExpression>(
                    std::move(expression),
                    BoundExpressionKind::Like
                )
            );
        case BoundExpressionKind::Cast:
            return rewrite_cast(
                owning_downcast<BoundCastExpression>(
                    std::move(expression),
                    BoundExpressionKind::Cast
                )
            );
        default:
            std::unreachable();
        }
    }

private:
    [[nodiscard]]
    RewriteResult rewrite_unary(std::unique_ptr<BoundUnaryExpression> expression)
    {
        const auto op = expression->op();
        const auto type = expression->type();
        auto operand = rewrite(expression->take_operand());

        if (op == common::UnaryOperator::Not && is_boolean_literal(*operand.expression, true)) {
            return RewriteResult {
                .expression = make_boolean_literal(false),
                .constant_foldable = true,
            };
        }
        if (op == common::UnaryOperator::Not && is_boolean_literal(*operand.expression, false)) {
            return RewriteResult {
                .expression = make_boolean_literal(true),
                .constant_foldable = true,
            };
        }

        const auto constant_foldable = operand.constant_foldable;
        return try_fold_constant(
            RewriteResult {
                .expression =
                    std::make_unique<BoundUnaryExpression>(op, std::move(operand.expression), type),
                .constant_foldable = constant_foldable,
            }
        );
    }

    [[nodiscard]]
    RewriteResult rewrite_binary(std::unique_ptr<BoundBinaryExpression> expression)
    {
        const auto op = expression->op();
        const auto type = expression->type();
        auto left = rewrite(expression->take_left());
        auto right = rewrite(expression->take_right());

        if (op == common::BinaryOperator::And) {
            if (is_boolean_literal(*left.expression, true)) {
                return right;
            }
            if (is_boolean_literal(*left.expression, false)) {
                return left;
            }
            if (is_boolean_literal(*right.expression, true)) {
                return left;
            }
        } else if (op == common::BinaryOperator::Or) {
            if (is_boolean_literal(*left.expression, true)) {
                return left;
            }
            if (is_boolean_literal(*left.expression, false)) {
                return right;
            }
            if (is_boolean_literal(*right.expression, false)) {
                return left;
            }
        }

        const auto constant_foldable = left.constant_foldable && right.constant_foldable;
        return try_fold_constant(
            RewriteResult {
                .expression = std::make_unique<BoundBinaryExpression>(
                    std::move(left.expression),
                    op,
                    std::move(right.expression),
                    type
                ),
                .constant_foldable = constant_foldable,
            }
        );
    }

    [[nodiscard]]
    RewriteResult rewrite_vector(std::unique_ptr<BoundVectorExpression> expression)
    {
        auto elements = expression->take_elements();
        for (auto & element : elements) {
            auto rewritten = rewrite(std::move(element));
            element = std::move(rewritten.expression);
        }
        return RewriteResult {
            .expression = std::make_unique<BoundVectorExpression>(std::move(elements)),
            .constant_foldable = false,
        };
    }

    [[nodiscard]]
    RewriteResult rewrite_function(std::unique_ptr<BoundFunctionExpression> expression)
    {
        auto function = expression->function();
        auto arguments = expression->take_arguments();
        for (auto & argument : arguments) {
            auto rewritten = rewrite(std::move(argument));
            argument = std::move(rewritten.expression);
        }
        return RewriteResult {
            .expression = std::make_unique<BoundFunctionExpression>(
                std::move(function),
                std::move(arguments)
            ),
            .constant_foldable = false,
        };
    }

    [[nodiscard]]
    RewriteResult rewrite_in(std::unique_ptr<BoundInExpression> expression)
    {
        auto value_expression = rewrite(expression->take_expression());
        auto values = expression->take_values();
        for (auto & value : values) {
            auto rewritten = rewrite(std::move(value));
            value = std::move(rewritten.expression);
        }
        return RewriteResult {
            .expression = std::make_unique<BoundInExpression>(
                std::move(value_expression.expression),
                std::move(values)
            ),
            .constant_foldable = false,
        };
    }

    [[nodiscard]]
    RewriteResult rewrite_between(std::unique_ptr<BoundBetweenExpression> expression)
    {
        auto value_expression = rewrite(expression->take_expression());
        auto lower = rewrite(expression->take_lower());
        auto upper = rewrite(expression->take_upper());
        return RewriteResult {
            .expression = std::make_unique<BoundBetweenExpression>(
                std::move(value_expression.expression),
                std::move(lower.expression),
                std::move(upper.expression)
            ),
            .constant_foldable = false,
        };
    }

    [[nodiscard]]
    RewriteResult rewrite_like(std::unique_ptr<BoundLikeExpression> expression)
    {
        auto value_expression = rewrite(expression->take_expression());
        auto pattern = rewrite(expression->take_pattern());
        return RewriteResult {
            .expression = std::make_unique<BoundLikeExpression>(
                std::move(value_expression.expression),
                std::move(pattern.expression)
            ),
            .constant_foldable = false,
        };
    }

    [[nodiscard]]
    RewriteResult rewrite_cast(std::unique_ptr<BoundCastExpression> expression)
    {
        const auto type = expression->type();
        auto child = rewrite(expression->take_expression());
        const auto constant_foldable = child.constant_foldable;
        return try_fold_constant(
            RewriteResult {
                .expression =
                    std::make_unique<BoundCastExpression>(std::move(child.expression), type),
                .constant_foldable = constant_foldable,
            }
        );
    }
};

} // namespace

bool is_boolean_literal(const binder::bound::BoundExpression & expression, bool value) noexcept
{
    if (expression.kind() != binder::bound::BoundExpressionKind::Literal ||
        expression.type().id != common::LogicalTypeId::Boolean) {
        return false;
    }

    const auto & literal = static_cast<const binder::bound::BoundLiteralExpression &>(expression);
    const auto * literal_value = std::get_if<bool>(&literal.value().data());
    return literal_value != nullptr && *literal_value == value;
}

std::unique_ptr<binder::bound::BoundExpression> rewrite_expression(
    std::unique_ptr<binder::bound::BoundExpression> expression
)
{
    assert(expression != nullptr);
    ExpressionRewriter rewriter;
    auto result = rewriter.rewrite(std::move(expression));
    return std::move(result.expression);
}

} // namespace litedb::core::optimizer::detail

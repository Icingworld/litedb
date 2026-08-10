#include "core/optimizer/detail/expression_rewriter.hpp"

#include <cassert>
#include <memory>
#include <optional>
#include <utility>
#include <variant>

#include "core/binder/bound/dispatcher/expression_dispatcher.hpp"
#include "core/binder/bound/expression/bound_between_expression.hpp"
#include "core/binder/bound/expression/bound_binary_expression.hpp"
#include "core/binder/bound/expression/bound_cast_expression.hpp"
#include "core/binder/bound/expression/bound_column_ref_expression.hpp"
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
#include "core/optimizer/optimizer.hpp"

namespace litedb::core::optimizer::detail
{
namespace
{

using binder::bound::BoundBetweenExpression;
using binder::bound::BoundBinaryExpression;
using binder::bound::BoundCastExpression;
using binder::bound::BoundColumnRefExpression;
using binder::bound::BoundExpression;
using binder::bound::BoundFunctionExpression;
using binder::bound::BoundInExpression;
using binder::bound::BoundLikeExpression;
using binder::bound::BoundLiteralExpression;
using binder::bound::BoundNullExpression;
using binder::bound::BoundUnaryExpression;
using binder::bound::BoundVectorExpression;

// 直接转发传入的表达式
template <typename ExpressionType>
[[nodiscard]]
std::unique_ptr<BoundExpression> reclaim_expression(
    ExpressionType & expression
) noexcept
{
    return std::unique_ptr<BoundExpression>(
        std::unique_ptr<ExpressionType>(&expression)
    );
}

// 常量可折叠性检查器
class ConstantFoldability final
    : private binder::bound::ConstBoundExpressionDispatcher<
          ConstantFoldability,
          bool
      >
{
    friend binder::bound::ConstBoundExpressionDispatcher<
        ConstantFoldability,
        bool
    >;

public:
    // 检查表达式是否可常量折叠
    [[nodiscard]]
    bool check(const BoundExpression & expression)
    {
        return dispatch_expression(expression);
    }

private:
    [[nodiscard]]
    bool visit_literal_expression(const BoundLiteralExpression &)
    {
        return true;
    }

    [[nodiscard]]
    bool visit_null_expression(const BoundNullExpression &)
    {
        return true;
    }

    [[nodiscard]]
    bool visit_column_ref_expression(const BoundColumnRefExpression &)
    {
        return false;
    }

    [[nodiscard]]
    bool visit_unary_expression(const BoundUnaryExpression & expression)
    {
        return check(expression.operand());
    }

    [[nodiscard]]
    bool visit_binary_expression(const BoundBinaryExpression & expression)
    {
        return check(expression.left()) && check(expression.right());
    }

    [[nodiscard]]
    bool visit_vector_expression(const BoundVectorExpression &)
    {
        return false;
    }

    [[nodiscard]]
    bool visit_function_expression(const BoundFunctionExpression &)
    {
        return false;
    }

    [[nodiscard]]
    bool visit_in_expression(const BoundInExpression &)
    {
        return false;
    }

    [[nodiscard]]
    bool visit_between_expression(const BoundBetweenExpression &)
    {
        return false;
    }

    [[nodiscard]]
    bool visit_like_expression(const BoundLikeExpression &)
    {
        return false;
    }

    [[nodiscard]]
    bool visit_cast_expression(const BoundCastExpression & expression)
    {
        return check(expression.expression());
    }
};

// 判断表达式是否可常量折叠
[[nodiscard]]
bool is_constant_foldable(const BoundExpression & expression)
{
    ConstantFoldability checker;
    return checker.check(expression);
}

// 构造布尔字面量表达式
[[nodiscard]]
std::unique_ptr<BoundExpression> make_boolean_literal(bool value)
{
    return std::make_unique<BoundLiteralExpression>(
        common::LogicalType {
            common::LogicalTypeId::Boolean,
            std::nullopt
        },
        common::Value {common::ValueData {value}}
    );
}

// 尝试常量折叠
[[nodiscard]]
std::unique_ptr<BoundExpression> try_fold_constant(
    std::unique_ptr<BoundExpression> expression,
    const OptimizerOptions & options
)
{
    assert(expression != nullptr);
    if (!options.enable_constant_folding || !is_constant_foldable(*expression)) {
        return expression;
    }

    auto value = evaluator::ExpressionEvaluator::evaluate_constant(*expression);
    if (!value.has_value()) {
        return expression;
    }

    const auto type = expression->type();
    if (value->is_null()) {
        return std::make_unique<BoundNullExpression>(type);
    }

    return std::make_unique<BoundLiteralExpression>(type, std::move(*value));
}

// 表达式重写器
class ExpressionRewriter final
    : private binder::bound::MutableBoundExpressionDispatcher<
          ExpressionRewriter,
          std::unique_ptr<BoundExpression>
      >
{
    friend binder::bound::MutableBoundExpressionDispatcher<
        ExpressionRewriter,
        std::unique_ptr<BoundExpression>
    >;

public:
    explicit ExpressionRewriter(const OptimizerOptions & options) noexcept
        : options_(options)
    {
    }

    // 重写表达式
    [[nodiscard]]
    std::unique_ptr<BoundExpression> rewrite(
        std::unique_ptr<BoundExpression> expression
    )
    {
        assert(expression != nullptr);
        return dispatch_expression(*expression.release());
    }

private:
    [[nodiscard]]
    std::unique_ptr<BoundExpression> visit_literal_expression(
        BoundLiteralExpression & expression
    )
    {
        return reclaim_expression(expression);
    }

    [[nodiscard]]
    std::unique_ptr<BoundExpression> visit_null_expression(
        BoundNullExpression & expression
    )
    {
        return reclaim_expression(expression);
    }

    [[nodiscard]]
    std::unique_ptr<BoundExpression> visit_column_ref_expression(
        BoundColumnRefExpression & expression
    )
    {
        return reclaim_expression(expression);
    }

    [[nodiscard]]
    std::unique_ptr<BoundExpression> visit_unary_expression(
        BoundUnaryExpression & expression
    )
    {
        const auto op = expression.op();
        const auto type = expression.type();
        auto operand = rewrite(expression.take_operand());

        if (options_.enable_boolean_simplification
            && op == common::UnaryOperator::Not
            && is_boolean_literal(*operand, true)) {
            return make_boolean_literal(false);
        }
        if (options_.enable_boolean_simplification
            && op == common::UnaryOperator::Not
            && is_boolean_literal(*operand, false)) {
            return make_boolean_literal(true);
        }

        return try_fold_constant(
            std::make_unique<BoundUnaryExpression>(
                op,
                std::move(operand),
                type
            ),
            options_
        );
    }

    [[nodiscard]]
    std::unique_ptr<BoundExpression> visit_binary_expression(
        BoundBinaryExpression & expression
    )
    {
        const auto op = expression.op();
        const auto type = expression.type();
        auto left = rewrite(expression.take_left());
        auto right = rewrite(expression.take_right());

        if (options_.enable_boolean_simplification) {
            if (op == common::BinaryOperator::And) {
                if (is_boolean_literal(*left, true)) {
                    return right;
                }
                if (is_boolean_literal(*left, false)) {
                    return left;
                }
                if (is_boolean_literal(*right, true)) {
                    return left;
                }
            } else if (op == common::BinaryOperator::Or) {
                if (is_boolean_literal(*left, true)) {
                    return left;
                }
                if (is_boolean_literal(*left, false)) {
                    return right;
                }
                if (is_boolean_literal(*right, false)) {
                    return left;
                }
            }
        }

        return try_fold_constant(
            std::make_unique<BoundBinaryExpression>(
                std::move(left),
                op,
                std::move(right),
                type
            ),
            options_
        );
    }

    [[nodiscard]]
    std::unique_ptr<BoundExpression> visit_vector_expression(
        BoundVectorExpression & expression
    )
    {
        auto elements = expression.take_elements();
        for (auto & element : elements) {
            element = rewrite(std::move(element));
        }
        return std::make_unique<BoundVectorExpression>(std::move(elements));
    }

    [[nodiscard]]
    std::unique_ptr<BoundExpression> visit_function_expression(
        BoundFunctionExpression & expression
    )
    {
        auto function = expression.function();
        auto arguments = expression.take_arguments();
        for (auto & argument : arguments) {
            argument = rewrite(std::move(argument));
        }
        return std::make_unique<BoundFunctionExpression>(
            std::move(function),
            std::move(arguments)
        );
    }

    [[nodiscard]]
    std::unique_ptr<BoundExpression> visit_in_expression(
        BoundInExpression & expression
    )
    {
        auto value_expression = rewrite(expression.take_expression());
        auto values = expression.take_values();
        for (auto & value : values) {
            value = rewrite(std::move(value));
        }
        return std::make_unique<BoundInExpression>(
            std::move(value_expression),
            std::move(values)
        );
    }

    [[nodiscard]]
    std::unique_ptr<BoundExpression> visit_between_expression(
        BoundBetweenExpression & expression
    )
    {
        auto value_expression = rewrite(expression.take_expression());
        auto lower = rewrite(expression.take_lower());
        auto upper = rewrite(expression.take_upper());
        return std::make_unique<BoundBetweenExpression>(
            std::move(value_expression),
            std::move(lower),
            std::move(upper)
        );
    }

    [[nodiscard]]
    std::unique_ptr<BoundExpression> visit_like_expression(
        BoundLikeExpression & expression
    )
    {
        auto value_expression = rewrite(expression.take_expression());
        auto pattern = rewrite(expression.take_pattern());
        return std::make_unique<BoundLikeExpression>(
            std::move(value_expression),
            std::move(pattern)
        );
    }

    [[nodiscard]]
    std::unique_ptr<BoundExpression> visit_cast_expression(
        BoundCastExpression & expression
    )
    {
        const auto type = expression.type();
        auto child = rewrite(expression.take_expression());
        return try_fold_constant(
            std::make_unique<BoundCastExpression>(std::move(child), type),
            options_
        );
    }

private:
    const OptimizerOptions & options_;
};

} // namespace

bool is_boolean_literal(
    const BoundExpression & expression,
    bool value
) noexcept
{
    if (expression.kind() != binder::bound::BoundExpressionKind::Literal
        || expression.type().id != common::LogicalTypeId::Boolean) {
        return false;
    }

    const auto & literal =
        static_cast<const BoundLiteralExpression &>(expression);
    const auto * literal_value = std::get_if<bool>(&literal.value().data());
    return literal_value != nullptr && *literal_value == value;
}

std::unique_ptr<BoundExpression> rewrite_expression(
    std::unique_ptr<BoundExpression> expression,
    const OptimizerOptions & options
)
{
    assert(expression != nullptr);
    ExpressionRewriter rewriter {options};
    return rewriter.rewrite(std::move(expression));
}

} // namespace litedb::core::optimizer::detail

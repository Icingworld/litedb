#include "core/binder/detail/expression_binder.hpp"

#include <utility>
#include <vector>

#include "core/binder/binder_helper.hpp"
#include "core/binder/bound/expression/bound_between_expression.hpp"
#include "core/binder/bound/expression/bound_in_expression.hpp"
#include "core/binder/bound/expression/bound_like_expression.hpp"

namespace litedb::core::binder::detail
{

using namespace litedb::core::binder::bound;
using namespace litedb::core::common;
using namespace litedb::core::parser::ast;

std::expected<std::unique_ptr<bound::BoundExpression>, BinderError>
ExpressionBinder::visit_in_expression(const InExpression & expression)
{
    auto target = bind(expression.expression());
    if (!target.has_value()) [[unlikely]] {
        return std::unexpected(std::move(target.error()));
    }

    std::vector<std::unique_ptr<BoundExpression>> values;
    auto comparison_type = (*target)->type();
    for (const auto & value : expression.values()) {
        auto bound_value = bind(*value);
        if (!bound_value.has_value()) [[unlikely]] {
            return std::unexpected(std::move(bound_value.error()));
        }
        if (!can_compare((*target)->type(), (*bound_value)->type(), common::BinaryOperator::Equal))
            [[unlikely]] {
            return std::unexpected(make_binder_error(
                BinderErrorCode::InvalidType,
                value->location(),
                "IN value is not comparable with target expression"
            ));
        }
        if (is_numeric(comparison_type) && is_numeric((*bound_value)->type())) {
            comparison_type = common_numeric_type(comparison_type, (*bound_value)->type());
        }
        values.push_back(std::move(*bound_value));
    }

    if (is_numeric(comparison_type)) {
        target = cast_if_needed(std::move(*target), comparison_type);
        for (auto & value : values) {
            value = cast_if_needed(std::move(value), comparison_type);
        }
    }

    return std::make_unique<BoundInExpression>(std::move(*target), std::move(values));
}

std::expected<std::unique_ptr<bound::BoundExpression>, BinderError>
ExpressionBinder::visit_between_expression(const BetweenExpression & expression)
{
    auto target = bind(expression.expression());
    if (!target.has_value()) [[unlikely]] {
        return std::unexpected(std::move(target.error()));
    }
    auto lower = bind(expression.lower());
    if (!lower.has_value()) [[unlikely]] {
        return std::unexpected(std::move(lower.error()));
    }
    auto upper = bind(expression.upper());
    if (!upper.has_value()) [[unlikely]] {
        return std::unexpected(std::move(upper.error()));
    }

    if (!can_compare(
            (*target)->type(),
            (*lower)->type(),
            common::BinaryOperator::GreaterThanOrEqual
        ) ||
        !can_compare((*target)->type(), (*upper)->type(), common::BinaryOperator::LessThanOrEqual))
        [[unlikely]] {
        return std::unexpected(make_binder_error(
            BinderErrorCode::InvalidType,
            expression.location(),
            "BETWEEN bounds are not comparable with target expression"
        ));
    }

    if (is_numeric((*target)->type()) && is_numeric((*lower)->type()) &&
        is_numeric((*upper)->type())) {
        const auto comparison_type = common_numeric_type(
            common_numeric_type((*target)->type(), (*lower)->type()),
            (*upper)->type()
        );
        target = cast_if_needed(std::move(*target), comparison_type);
        lower = cast_if_needed(std::move(*lower), comparison_type);
        upper = cast_if_needed(std::move(*upper), comparison_type);
    }

    return std::make_unique<BoundBetweenExpression>(
        std::move(*target),
        std::move(*lower),
        std::move(*upper)
    );
}

std::expected<std::unique_ptr<bound::BoundExpression>, BinderError>
ExpressionBinder::visit_like_expression(const LikeExpression & expression)
{
    auto target = bind(expression.expression());
    if (!target.has_value()) [[unlikely]] {
        return std::unexpected(std::move(target.error()));
    }
    auto pattern = bind(expression.pattern());
    if (!pattern.has_value()) [[unlikely]] {
        return std::unexpected(std::move(pattern.error()));
    }

    if (!is_varchar((*target)->type()) || !is_varchar((*pattern)->type())) [[unlikely]] {
        return std::unexpected(make_binder_error(
            BinderErrorCode::InvalidType,
            expression.location(),
            "LIKE operands must be VARCHAR"
        ));
    }

    return std::make_unique<BoundLikeExpression>(std::move(*target), std::move(*pattern));
}

} // namespace litedb::core::binder::detail

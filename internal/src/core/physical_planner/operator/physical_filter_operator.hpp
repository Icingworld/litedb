#pragma once

#include <memory>
#include <utility>

#include "core/binder/bound/expression/bound_expression.hpp"
#include "core/physical_planner/operator/physical_unary_operator.hpp"

namespace litedb::core::physical_planner::op
{

class FilterOperator final : public PhysicalUnaryOperator
{
public:
    FilterOperator(
        std::unique_ptr<PhysicalOperator> child,
        std::unique_ptr<binder::bound::BoundExpression> predicate
    ) noexcept
        : PhysicalUnaryOperator(PhysicalOperatorKind::Filter, std::move(child))
        , predicate_(std::move(predicate))
    {
    }

    [[nodiscard]] const binder::bound::BoundExpression & predicate() const noexcept
    {
        return *predicate_;
    }

    [[nodiscard]] const binder::bound::BoundExpression * predicate_ptr() const noexcept
    {
        return predicate_.get();
    }

private:
    std::unique_ptr<binder::bound::BoundExpression> predicate_;
};

} // namespace litedb::core::physical_planner::op

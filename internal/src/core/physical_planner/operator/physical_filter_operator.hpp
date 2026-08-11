#pragma once

#include <memory>

#include "core/binder/bound/expression/bound_expression.hpp"
#include "core/physical_planner/operator/physical_unary_operator.hpp"

namespace litedb::core::physical_planner::op
{

// 过滤算子
class FilterOperator final : public PhysicalUnaryOperator
{
public:
    FilterOperator(
        std::unique_ptr<PhysicalOperator> child,
        std::unique_ptr<binder::bound::BoundExpression> predicate
    ) noexcept;

public:
    // 获取谓词
    [[nodiscard]]
    const binder::bound::BoundExpression & predicate() const noexcept;

private:
    std::unique_ptr<binder::bound::BoundExpression> predicate_;
};

} // namespace litedb::core::physical_planner::op

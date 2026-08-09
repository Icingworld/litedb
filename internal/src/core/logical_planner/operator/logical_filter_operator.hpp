#pragma once

#include <memory>

#include "core/binder/bound/expression/bound_expression.hpp"
#include "core/logical_planner/operator/logical_unary_operator.hpp"

namespace litedb::core::logical_planner::op
{

// 逻辑过滤算子
class LogicalFilterOperator final : public LogicalUnaryOperator
{
public:
    LogicalFilterOperator(
        std::unique_ptr<LogicalPlanOperator> child,
        std::unique_ptr<binder::bound::BoundExpression> predicate
    );

public:
    // 获取谓词
    [[nodiscard]]
    const binder::bound::BoundExpression & predicate() const noexcept;

    // 获取谓词所有权
    // 调用后 predicate() 不可调用；再次调用返回 nullptr
    [[nodiscard]]
    std::unique_ptr<binder::bound::BoundExpression> take_predicate() noexcept;

private:
    std::unique_ptr<binder::bound::BoundExpression> predicate_;
};

} // namespace litedb::core::logical_planner::op

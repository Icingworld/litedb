#pragma once

#include <memory>

#include "core/binder/bound/expression/bound_expression.hpp"
#include "core/logical_planner/operator/logical_unary_operator.hpp"

namespace litedb::core::logical_planner::op
{

/**
 * @brief 逻辑过滤算子
 */
class LogicalFilterOperator final : public LogicalUnaryOperator
{
public:
    LogicalFilterOperator(
        std::unique_ptr<LogicalPlanOperator> child,
        std::unique_ptr<binder::bound::BoundExpression> predicate
    );

public:
    /**
     * @brief 获取谓词
     * @return 谓词
     */
    [[nodiscard]]
    const binder::bound::BoundExpression & predicate() const noexcept;

private:
    std::unique_ptr<binder::bound::BoundExpression> predicate_;   ///< 谓词
};

} // namespace litedb::core::logical_planner::op

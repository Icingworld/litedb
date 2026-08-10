#include "core/optimizer/optimizer.hpp"

#include <cassert>
#include <memory>
#include <utility>

#include "core/logical_planner/operator/logical_filter_operator.hpp"
#include "core/logical_planner/operator/logical_limit_operator.hpp"
#include "core/logical_planner/operator/logical_order_by_operator.hpp"
#include "core/logical_planner/operator/logical_projection_operator.hpp"
#include "core/logical_planner/plan/logical_plan.hpp"
#include "core/logical_planner/plan/mutation/delete_plan.hpp"
#include "core/logical_planner/plan/mutation/insert_plan.hpp"
#include "core/logical_planner/plan/mutation/update_plan.hpp"
#include "core/logical_planner/plan/query/query_plan.hpp"
#include "core/optimizer/detail/expression_rewriter.hpp"

namespace
{

template <typename Derived, typename Base, typename Kind>
[[nodiscard]]
std::unique_ptr<Derived> owning_downcast(std::unique_ptr<Base> value, Kind expected_kind) noexcept
{
    assert(value != nullptr);
    assert(value->kind() == expected_kind);
    return std::unique_ptr<Derived>(static_cast<Derived *>(value.release()));
}

class PlanRewriter final
{
public:
    [[nodiscard]]
    std::unique_ptr<litedb::core::logical_planner::plan::LogicalPlan> rewrite(
        std::unique_ptr<litedb::core::logical_planner::plan::LogicalPlan> plan
    )
    {
        using namespace litedb::core::logical_planner::plan;

        assert(plan != nullptr);
        switch (plan->kind()) {
        case LogicalPlanKind::Use:
        case LogicalPlanKind::CreateDatabase:
        case LogicalPlanKind::CreateCollection:
        case LogicalPlanKind::CreateIndex:
        case LogicalPlanKind::CreateVectorIndex:
        case LogicalPlanKind::DropDatabase:
        case LogicalPlanKind::DropCollection:
        case LogicalPlanKind::DropIndex:
        case LogicalPlanKind::DropVectorIndex:
        case LogicalPlanKind::ShowDatabases:
        case LogicalPlanKind::ShowCollections:
        case LogicalPlanKind::ShowIndexes:
        case LogicalPlanKind::ShowVectorIndexes:
        case LogicalPlanKind::DescribeCollection:
            return plan;
        case LogicalPlanKind::Insert:
            return rewrite_insert(
                owning_downcast<InsertPlan>(std::move(plan), LogicalPlanKind::Insert)
            );
        case LogicalPlanKind::Update:
            return rewrite_update(
                owning_downcast<UpdatePlan>(std::move(plan), LogicalPlanKind::Update)
            );
        case LogicalPlanKind::Delete:
            return rewrite_delete(
                owning_downcast<DeletePlan>(std::move(plan), LogicalPlanKind::Delete)
            );
        case LogicalPlanKind::Query:
            return rewrite_query(
                owning_downcast<QueryPlan>(std::move(plan), LogicalPlanKind::Query)
            );
        default:
            std::unreachable();
        }
    }

private:
    [[nodiscard]]
    std::unique_ptr<litedb::core::logical_planner::plan::LogicalPlan> rewrite_insert(
        std::unique_ptr<litedb::core::logical_planner::plan::InsertPlan> plan
    )
    {
        using litedb::core::logical_planner::plan::InsertPlan;

        const auto collection_id = plan->collection_id();
        auto values = plan->take_values();
        for (auto & value : values) {
            value = litedb::core::optimizer::detail::rewrite_expression(std::move(value));
        }
        return std::make_unique<InsertPlan>(collection_id, std::move(values));
    }

    [[nodiscard]]
    std::unique_ptr<litedb::core::logical_planner::plan::LogicalPlan> rewrite_update(
        std::unique_ptr<litedb::core::logical_planner::plan::UpdatePlan> plan
    )
    {
        using litedb::core::logical_planner::plan::UpdatePlan;

        const auto collection_id = plan->collection_id();
        auto root = rewrite_operator(plan->take_root_operator());
        auto assignments = plan->take_assignments();
        for (auto & assignment : assignments) {
            assignment.value = litedb::core::optimizer::detail::rewrite_expression(
                std::move(assignment.value)
            );
        }
        return std::make_unique<UpdatePlan>(collection_id, std::move(assignments), std::move(root));
    }

    [[nodiscard]]
    std::unique_ptr<litedb::core::logical_planner::plan::LogicalPlan> rewrite_delete(
        std::unique_ptr<litedb::core::logical_planner::plan::DeletePlan> plan
    )
    {
        using litedb::core::logical_planner::plan::DeletePlan;

        const auto collection_id = plan->collection_id();
        auto root = rewrite_operator(plan->take_root_operator());
        return std::make_unique<DeletePlan>(collection_id, std::move(root));
    }

    [[nodiscard]]
    std::unique_ptr<litedb::core::logical_planner::plan::LogicalPlan> rewrite_query(
        std::unique_ptr<litedb::core::logical_planner::plan::QueryPlan> plan
    )
    {
        using litedb::core::logical_planner::plan::QueryPlan;

        auto root = rewrite_operator(plan->take_root_operator());
        return std::make_unique<QueryPlan>(std::move(root));
    }

    [[nodiscard]]
    std::unique_ptr<litedb::core::logical_planner::op::LogicalPlanOperator> rewrite_operator(
        std::unique_ptr<litedb::core::logical_planner::op::LogicalPlanOperator> op
    )
    {
        using namespace litedb::core::logical_planner::op;

        assert(op != nullptr);
        switch (op->kind()) {
        case LogicalPlanOperatorKind::Scan:
            return op;
        case LogicalPlanOperatorKind::Filter:
            return rewrite_filter(
                owning_downcast<LogicalFilterOperator>(
                    std::move(op),
                    LogicalPlanOperatorKind::Filter
                )
            );
        case LogicalPlanOperatorKind::Projection:
            return rewrite_projection(
                owning_downcast<LogicalProjectionOperator>(
                    std::move(op),
                    LogicalPlanOperatorKind::Projection
                )
            );
        case LogicalPlanOperatorKind::OrderBy:
            return rewrite_order_by(
                owning_downcast<LogicalOrderByOperator>(
                    std::move(op),
                    LogicalPlanOperatorKind::OrderBy
                )
            );
        case LogicalPlanOperatorKind::Limit:
            return rewrite_limit(
                owning_downcast<LogicalLimitOperator>(std::move(op), LogicalPlanOperatorKind::Limit)
            );
        default:
            std::unreachable();
        }
    }

    [[nodiscard]]
    std::unique_ptr<litedb::core::logical_planner::op::LogicalPlanOperator> rewrite_filter(
        std::unique_ptr<litedb::core::logical_planner::op::LogicalFilterOperator> op
    )
    {
        using litedb::core::logical_planner::op::LogicalFilterOperator;

        auto child = rewrite_operator(op->take_child());
        auto predicate = litedb::core::optimizer::detail::rewrite_expression(op->take_predicate());
        if (litedb::core::optimizer::detail::is_boolean_literal(*predicate, true)) {
            return child;
        }
        return std::make_unique<LogicalFilterOperator>(std::move(child), std::move(predicate));
    }

    [[nodiscard]]
    std::unique_ptr<litedb::core::logical_planner::op::LogicalPlanOperator> rewrite_projection(
        std::unique_ptr<litedb::core::logical_planner::op::LogicalProjectionOperator> op
    )
    {
        using litedb::core::logical_planner::op::LogicalProjectionOperator;

        auto child = rewrite_operator(op->take_child());
        auto projections = op->take_projections();
        for (auto & item : projections) {
            item.expression = litedb::core::optimizer::detail::rewrite_expression(
                std::move(item.expression)
            );
        }
        return std::make_unique<LogicalProjectionOperator>(
            std::move(child),
            std::move(projections)
        );
    }

    [[nodiscard]]
    std::unique_ptr<litedb::core::logical_planner::op::LogicalPlanOperator> rewrite_order_by(
        std::unique_ptr<litedb::core::logical_planner::op::LogicalOrderByOperator> op
    )
    {
        using litedb::core::logical_planner::op::LogicalOrderByOperator;

        auto child = rewrite_operator(op->take_child());
        auto order_items = op->take_order_by();
        for (auto & item : order_items) {
            item.expression = litedb::core::optimizer::detail::rewrite_expression(
                std::move(item.expression)
            );
        }
        return std::make_unique<LogicalOrderByOperator>(std::move(child), std::move(order_items));
    }

    [[nodiscard]]
    std::unique_ptr<litedb::core::logical_planner::op::LogicalPlanOperator> rewrite_limit(
        std::unique_ptr<litedb::core::logical_planner::op::LogicalLimitOperator> op
    )
    {
        using litedb::core::logical_planner::op::LogicalLimitOperator;

        const auto limit = op->limit();
        const auto offset = op->offset();
        auto child = rewrite_operator(op->take_child());
        return std::make_unique<LogicalLimitOperator>(std::move(child), limit, offset);
    }
};

} // namespace

namespace litedb::core::optimizer
{

Optimizer::Optimizer(OptimizerOptions options) noexcept
    : options_(options)
{}

std::unique_ptr<logical_planner::plan::LogicalPlan> Optimizer::optimize(
    std::unique_ptr<logical_planner::plan::LogicalPlan> plan
)
{
    assert(plan != nullptr);
    if (!options_.enabled) {
        return plan;
    }
    return PlanRewriter {}.rewrite(std::move(plan));
}

} // namespace litedb::core::optimizer

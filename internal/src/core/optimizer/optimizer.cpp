#include "core/optimizer/optimizer.hpp"

#include <cassert>
#include <utility>

#include "core/logical_planner/operator/logical_filter_operator.hpp"
#include "core/logical_planner/operator/logical_limit_operator.hpp"
#include "core/logical_planner/operator/logical_order_by_operator.hpp"
#include "core/logical_planner/operator/logical_projection_operator.hpp"
#include "core/logical_planner/operator/logical_scan_operator.hpp"
#include "core/logical_planner/plan/logical_plan.hpp"
#include "core/optimizer/detail/expression_rewriter.hpp"

namespace
{

/**
 * @brief 直接转发传入的计划
 * @tparam PlanType 计划类型
 * @param plan 计划
 * @return 计划
 * @details 不含有需要优化的算子，直接转发
 */
template <typename PlanType>
std::unique_ptr<litedb::core::logical_planner::plan::LogicalPlan>
reclaim_plan(PlanType & plan) noexcept
{
    return std::unique_ptr<
        litedb::core::logical_planner::plan::LogicalPlan
    >(
        std::unique_ptr<PlanType>(&plan)
    );
}

} // namespace

namespace litedb::core::optimizer
{

Optimizer::Optimizer(OptimizerOptions options) noexcept
    : options_(options)
{
}

std::unique_ptr<logical_planner::plan::LogicalPlan> Optimizer::optimize(
    std::unique_ptr<logical_planner::plan::LogicalPlan> plan
)
{
    assert(plan != nullptr);
    if (!options_.enabled) {
        return plan;
    }
    return dispatch_plan(*plan.release());
}

std::unique_ptr<logical_planner::plan::LogicalPlan>
Optimizer::visit_use_plan(logical_planner::plan::UsePlan & plan)
{
    return reclaim_plan(plan);
}

std::unique_ptr<logical_planner::plan::LogicalPlan>
Optimizer::visit_create_database_plan(
    logical_planner::plan::CreateDatabasePlan & plan
)
{
    return reclaim_plan(plan);
}

std::unique_ptr<logical_planner::plan::LogicalPlan>
Optimizer::visit_create_collection_plan(
    logical_planner::plan::CreateCollectionPlan & plan
)
{
    return reclaim_plan(plan);
}

std::unique_ptr<logical_planner::plan::LogicalPlan>
Optimizer::visit_create_index_plan(
    logical_planner::plan::CreateIndexPlan & plan
)
{
    return reclaim_plan(plan);
}

std::unique_ptr<logical_planner::plan::LogicalPlan>
Optimizer::visit_create_vector_index_plan(
    logical_planner::plan::CreateVectorIndexPlan & plan
)
{
    return reclaim_plan(plan);
}

std::unique_ptr<logical_planner::plan::LogicalPlan>
Optimizer::visit_drop_database_plan(
    logical_planner::plan::DropDatabasePlan & plan
)
{
    return reclaim_plan(plan);
}

std::unique_ptr<logical_planner::plan::LogicalPlan>
Optimizer::visit_drop_collection_plan(
    logical_planner::plan::DropCollectionPlan & plan
)
{
    return reclaim_plan(plan);
}

std::unique_ptr<logical_planner::plan::LogicalPlan>
Optimizer::visit_drop_index_plan(logical_planner::plan::DropIndexPlan & plan)
{
    return reclaim_plan(plan);
}

std::unique_ptr<logical_planner::plan::LogicalPlan>
Optimizer::visit_drop_vector_index_plan(
    logical_planner::plan::DropVectorIndexPlan & plan
)
{
    return reclaim_plan(plan);
}

std::unique_ptr<logical_planner::plan::LogicalPlan>
Optimizer::visit_show_databases_plan(
    logical_planner::plan::ShowDatabasesPlan & plan
)
{
    return reclaim_plan(plan);
}

std::unique_ptr<logical_planner::plan::LogicalPlan>
Optimizer::visit_show_collections_plan(
    logical_planner::plan::ShowCollectionsPlan & plan
)
{
    return reclaim_plan(plan);
}

std::unique_ptr<logical_planner::plan::LogicalPlan>
Optimizer::visit_show_indexes_plan(
    logical_planner::plan::ShowIndexesPlan & plan
)
{
    return reclaim_plan(plan);
}

std::unique_ptr<logical_planner::plan::LogicalPlan>
Optimizer::visit_show_vector_indexes_plan(
    logical_planner::plan::ShowVectorIndexesPlan & plan
)
{
    return reclaim_plan(plan);
}

std::unique_ptr<logical_planner::plan::LogicalPlan>
Optimizer::visit_describe_collection_plan(
    logical_planner::plan::DescribeCollectionPlan & plan
)
{
    return reclaim_plan(plan);
}

std::unique_ptr<logical_planner::plan::LogicalPlan>
Optimizer::visit_insert_plan(logical_planner::plan::InsertPlan & plan)
{
    using logical_planner::plan::InsertPlan;

    std::unique_ptr<InsertPlan> owned(&plan);
    const auto collection_id = owned->collection_id();
    auto values = owned->take_values();
    // 重写 InsertPlan 中的值表达式
    for (auto & value : values) {
        value = detail::rewrite_expression(std::move(value), options_);
    }
    return std::make_unique<InsertPlan>(collection_id, std::move(values));
}

std::unique_ptr<logical_planner::plan::LogicalPlan>
Optimizer::visit_update_plan(logical_planner::plan::UpdatePlan & plan)
{
    using logical_planner::plan::UpdatePlan;

    std::unique_ptr<UpdatePlan> owned(&plan);
    const auto collection_id = owned->collection_id();
    // 重建算子树
    auto root = rewrite_operator(owned->take_root_operator());
    auto assignments = owned->take_assignments();
    // 重写 UpdatePlan 中的赋值表达式
    for (auto & assignment : assignments) {
        assignment.value = detail::rewrite_expression(
            std::move(assignment.value),
            options_
        );
    }
    return std::make_unique<UpdatePlan>(
        collection_id,
        std::move(assignments),
        std::move(root)
    );
}

std::unique_ptr<logical_planner::plan::LogicalPlan>
Optimizer::visit_delete_plan(logical_planner::plan::DeletePlan & plan)
{
    using logical_planner::plan::DeletePlan;

    std::unique_ptr<DeletePlan> owned(&plan);
    const auto collection_id = owned->collection_id();
    // 重建算子树
    auto root = rewrite_operator(owned->take_root_operator());
    return std::make_unique<DeletePlan>(collection_id, std::move(root));
}

std::unique_ptr<logical_planner::plan::LogicalPlan>
Optimizer::visit_query_plan(logical_planner::plan::QueryPlan & plan)
{
    using logical_planner::plan::QueryPlan;

    std::unique_ptr<QueryPlan> owned(&plan);
    // 重建算子树
    auto root = rewrite_operator(owned->take_root_operator());
    return std::make_unique<QueryPlan>(std::move(root));
}

std::unique_ptr<logical_planner::op::LogicalPlanOperator>
Optimizer::rewrite_operator(
    std::unique_ptr<logical_planner::op::LogicalPlanOperator> op
)
{
    assert(op != nullptr);
    return dispatch_operator(*op.release());
}

std::unique_ptr<logical_planner::op::LogicalPlanOperator>
Optimizer::visit_scan_operator(
    logical_planner::op::LogicalScanOperator & op
)
{
    return std::unique_ptr<logical_planner::op::LogicalPlanOperator>(&op);
}

std::unique_ptr<logical_planner::op::LogicalPlanOperator>
Optimizer::visit_filter_operator(
    logical_planner::op::LogicalFilterOperator & op
)
{
    using logical_planner::op::LogicalFilterOperator;

    std::unique_ptr<LogicalFilterOperator> owned(&op);
    // 重建子算子树
    auto child = rewrite_operator(owned->take_child());
    // 重写谓词表达式
    auto predicate = detail::rewrite_expression(owned->take_predicate(), options_);
    // Filter 的放置由 LogicalPlanner 负责；本趟只重写其内容
    // 如果启用了过滤消除，并且谓词表达式为真，则删除本 Filter 算子，直接返回子算子树
    // 例如：WHERE true
    if (options_.enable_filter_elimination &&
        detail::is_boolean_literal(*predicate, true)) {
        return child;
    }
    return std::make_unique<LogicalFilterOperator>(
        std::move(child),
        std::move(predicate)
    );
}

std::unique_ptr<logical_planner::op::LogicalPlanOperator>
Optimizer::visit_projection_operator(
    logical_planner::op::LogicalProjectionOperator & op
)
{
    using logical_planner::op::LogicalProjectionOperator;

    std::unique_ptr<LogicalProjectionOperator> owned(&op);
    // 重建子算子树
    auto child = rewrite_operator(owned->take_child());
    auto projections = owned->take_projections();
    // 重写投影表达式
    for (auto & item : projections) {
        item.expression = detail::rewrite_expression(
            std::move(item.expression),
            options_
        );
    }
    return std::make_unique<LogicalProjectionOperator>(
        std::move(child),
        std::move(projections)
    );
}

std::unique_ptr<logical_planner::op::LogicalPlanOperator>
Optimizer::visit_order_by_operator(
    logical_planner::op::LogicalOrderByOperator & op
)
{
    using logical_planner::op::LogicalOrderByOperator;

    std::unique_ptr<LogicalOrderByOperator> owned(&op);
    // 重建子算子树
    auto child = rewrite_operator(owned->take_child());
    // 重写排序表达式
    auto order_items = owned->take_order_by();
    for (auto & item : order_items) {
        item.expression = detail::rewrite_expression(
            std::move(item.expression),
            options_
        );
    }
    return std::make_unique<LogicalOrderByOperator>(
        std::move(child),
        std::move(order_items)
    );
}

std::unique_ptr<logical_planner::op::LogicalPlanOperator>
Optimizer::visit_limit_operator(
    logical_planner::op::LogicalLimitOperator & op
)
{
    using logical_planner::op::LogicalLimitOperator;

    std::unique_ptr<LogicalLimitOperator> owned(&op);
    const auto limit_value = owned->limit();
    const auto offset_value = owned->offset();
    // 重建子算子树
    auto child = rewrite_operator(owned->take_child());
    return std::make_unique<LogicalLimitOperator>(
        std::move(child),
        limit_value,
        offset_value
    );
}

} // namespace litedb::core::optimizer

#include "core/planner/logical/logical_planner.hpp"

#include <memory>
#include <string>
#include <utility>

#include "core/planner/logical/logical_filter.hpp"
#include "core/planner/logical/logical_limit.hpp"
#include "core/planner/logical/logical_order_by.hpp"
#include "core/planner/logical/logical_projection.hpp"
#include "core/planner/logical/logical_scan.hpp"

namespace litedb::core::planner::logical
{

namespace
{

using namespace litedb::core::binder::bound;

/**
 * @brief 生成扫描节点
 * @param database_id 数据库ID
 * @param collection_id 集合ID
 * @param collection_name 集合名称
 * @param location 位置
 * @return 逻辑计划节点
 */
std::unique_ptr<LogicalPlanNode> scan_for(
    common::DatabaseId database_id,
    common::CollectionId collection_id,
    const std::string & collection_name,
    parser::ast::AstNodeLocation location
)
{
    return std::make_unique<LogicalScan>(database_id, collection_id, collection_name, location);
}

/**
 * @brief 生成过滤节点
 * @param input 输入
 * @param predicate 谓词
 * @param location 位置
 * @return 逻辑计划节点
 */
std::unique_ptr<LogicalPlanNode> apply_optional_filter(
    std::unique_ptr<LogicalPlanNode> input,
    std::unique_ptr<BoundExpression> predicate,
    parser::ast::AstNodeLocation location
)
{
    if (predicate == nullptr) {
        return input;
    }

    return std::make_unique<LogicalFilter>(std::move(input), std::move(predicate), location);
}

} // namespace

std::unique_ptr<LogicalPlanNode> LogicalPlanner::plan_select(BoundSelectStatement & statement) const
{
    auto current = scan_for(
        statement.database_id(),
        statement.collection_id(),
        statement.collection_name(),
        statement.location()
    );

    current = apply_optional_filter(std::move(current), statement.take_where(), statement.location());

    current = std::make_unique<LogicalProjection>(
        std::move(current),
        statement.take_projections(),
        statement.location()
    );

    auto order_by = statement.take_order_by();
    if (!order_by.empty()) {
        current = std::make_unique<LogicalOrderBy>(std::move(current), std::move(order_by), statement.location());
    }

    if (statement.limit().has_value() || statement.offset().has_value()) {
        current = std::make_unique<LogicalLimit>(
            std::move(current),
            statement.limit(),
            statement.offset(),
            statement.location()
        );
    }

    return current;
}

std::unique_ptr<LogicalPlanNode> LogicalPlanner::plan_update_input(BoundUpdateStatement & statement) const
{
    auto current = scan_for(
        statement.database_id(),
        statement.collection_id(),
        statement.collection_name(),
        statement.location()
    );

    return apply_optional_filter(std::move(current), statement.take_where(), statement.location());
}

std::unique_ptr<LogicalPlanNode> LogicalPlanner::plan_delete_input(BoundDeleteStatement & statement) const
{
    auto current = scan_for(
        statement.database_id(),
        statement.collection_id(),
        statement.collection_name(),
        statement.location()
    );

    return apply_optional_filter(std::move(current), statement.take_where(), statement.location());
}

} // namespace litedb::core::planner::logical

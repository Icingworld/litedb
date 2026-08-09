#pragma once

#include <memory>

namespace litedb::core::binder::bound
{

class BoundShowDatabasesStatement;
class BoundShowCollectionsStatement;
class BoundShowIndexesStatement;
class BoundShowVectorIndexesStatement;

} // namespace litedb::core::binder::bound

namespace litedb::core::logical_planner::plan
{

class LogicalPlan;

} // namespace litedb::core::logical_planner::plan

namespace litedb::core::logical_planner
{

// SHOW 语句逻辑计划工作器
class LogicalPlannerShowWorker
{
public:
    LogicalPlannerShowWorker() = default;

public:
    // 规划 SHOW DATABASES 语句
    [[nodiscard]]
    std::unique_ptr<plan::LogicalPlan>
    plan_show_databases(
        const binder::bound::BoundShowDatabasesStatement & statement
    );

    // 规划 SHOW COLLECTIONS 语句
    [[nodiscard]]
    std::unique_ptr<plan::LogicalPlan>
    plan_show_collections(
        const binder::bound::BoundShowCollectionsStatement & statement
    );

    // 规划 SHOW INDEXES 语句
    [[nodiscard]]
    std::unique_ptr<plan::LogicalPlan>
    plan_show_indexes(
        const binder::bound::BoundShowIndexesStatement & statement
    );

    // 规划 SHOW VINDEXES 语句
    [[nodiscard]]
    std::unique_ptr<plan::LogicalPlan>
    plan_show_vector_indexes(
        const binder::bound::BoundShowVectorIndexesStatement & statement
    );
};

} // namespace litedb::core::logical_planner

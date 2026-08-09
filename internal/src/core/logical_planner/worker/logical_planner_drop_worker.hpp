#pragma once

#include <memory>

namespace litedb::core::binder::bound
{

class BoundDropDatabaseStatement;
class BoundDropCollectionStatement;
class BoundDropIndexStatement;
class BoundDropVectorIndexStatement;

} // namespace litedb::core::binder::bound

namespace litedb::core::logical_planner::plan
{

class LogicalPlan;

} // namespace litedb::core::logical_planner::plan

namespace litedb::core::logical_planner
{


// DROP 语句逻辑计划工作器
class LogicalPlannerDropWorker
{
public:
    LogicalPlannerDropWorker() = default;

public:
    // 规划 DROP DATABASE 语句
    [[nodiscard]]
    std::unique_ptr<plan::LogicalPlan>
    plan_drop_database(
        const binder::bound::BoundDropDatabaseStatement & statement
    );

    // 规划 DROP COLLECTION 语句
    [[nodiscard]]
    std::unique_ptr<plan::LogicalPlan>
    plan_drop_collection(
        const binder::bound::BoundDropCollectionStatement & statement
    );

    // 规划 DROP INDEX 语句
    [[nodiscard]]
    std::unique_ptr<plan::LogicalPlan>
    plan_drop_index(
        const binder::bound::BoundDropIndexStatement & statement
    );

    // 规划 DROP VINDEX 语句
    [[nodiscard]]
    std::unique_ptr<plan::LogicalPlan>
    plan_drop_vector_index(
        const binder::bound::BoundDropVectorIndexStatement & statement
    );
};

} // namespace litedb::core::logical_planner

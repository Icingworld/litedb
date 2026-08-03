#pragma once

#include <expected>
#include <memory>

#include "core/logical_planner/logical_planner_error.hpp"

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

/**
 * @brief DROP 语句逻辑计划工作器
 */
class LogicalPlannerDropWorker
{
public:
    LogicalPlannerDropWorker() = default;

public:
    /**
     * @brief 规划 DROP DATABASE 语句
     * @param statement DROP DATABASE 语句
     * @return 逻辑计划
     */
    [[nodiscard]]
    std::expected<std::unique_ptr<plan::LogicalPlan>, LogicalPlannerError>
    plan_drop_database(
        const binder::bound::BoundDropDatabaseStatement & statement
    );

    /**
     * @brief 规划 DROP COLLECTION 语句
     * @param statement DROP COLLECTION 语句
     * @return 逻辑计划
     */
    [[nodiscard]]
    std::expected<std::unique_ptr<plan::LogicalPlan>, LogicalPlannerError>
    plan_drop_collection(
        const binder::bound::BoundDropCollectionStatement & statement
    );

    /**
     * @brief 规划 DROP INDEX 语句
     * @param statement DROP INDEX 语句
     * @return 逻辑计划
     */
    [[nodiscard]]
    std::expected<std::unique_ptr<plan::LogicalPlan>, LogicalPlannerError>
    plan_drop_index(
        const binder::bound::BoundDropIndexStatement & statement
    );

    /**
     * @brief 规划 DROP VINDEX 语句
     * @param statement DROP VINDEX 语句
     * @return 逻辑计划
     */
    [[nodiscard]]
    std::expected<std::unique_ptr<plan::LogicalPlan>, LogicalPlannerError>
    plan_drop_vector_index(
        const binder::bound::BoundDropVectorIndexStatement & statement
    );
};

} // namespace litedb::core::logical_planner

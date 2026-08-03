#pragma once

#include <expected>
#include <memory>

#include "core/logical_planner/logical_planner_error.hpp"

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

/**
 * @brief SHOW 语句逻辑计划工作器
 */
class LogicalPlannerShowWorker
{
public:
    LogicalPlannerShowWorker() = default;

public:
    /**
     * @brief 规划 SHOW DATABASES 语句
     * @param statement SHOW DATABASES 语句
     * @return 逻辑计划
     */
    [[nodiscard]]
    std::expected<std::unique_ptr<plan::LogicalPlan>, LogicalPlannerError>
    plan_show_databases(
        const binder::bound::BoundShowDatabasesStatement & statement
    );

    /**
     * @brief 规划 SHOW COLLECTIONS 语句
     * @param statement SHOW COLLECTIONS 语句
     * @return 逻辑计划
     */
    [[nodiscard]]
    std::expected<std::unique_ptr<plan::LogicalPlan>, LogicalPlannerError>
    plan_show_collections(
        const binder::bound::BoundShowCollectionsStatement & statement
    );

    /**
     * @brief 规划 SHOW INDEXES 语句
     * @param statement SHOW INDEXES 语句
     * @return 逻辑计划
     */
    [[nodiscard]]
    std::expected<std::unique_ptr<plan::LogicalPlan>, LogicalPlannerError>
    plan_show_indexes(
        const binder::bound::BoundShowIndexesStatement & statement
    );

    /**
     * @brief 规划 SHOW VINDEXES 语句
     * @param statement SHOW VINDEXES 语句
     * @return 逻辑计划
     */
    [[nodiscard]]
    std::expected<std::unique_ptr<plan::LogicalPlan>, LogicalPlannerError>
    plan_show_vector_indexes(
        const binder::bound::BoundShowVectorIndexesStatement & statement
    );
};

} // namespace litedb::core::logical_planner

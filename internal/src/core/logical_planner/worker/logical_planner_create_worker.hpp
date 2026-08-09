#pragma once

#include <memory>

namespace litedb::core::binder::bound
{

class BoundCreateDatabaseStatement;
class BoundCreateCollectionStatement;
class BoundCreateIndexStatement;
class BoundCreateVectorIndexStatement;

} // namespace litedb::core::binder::bound

namespace litedb::core::logical_planner::plan
{

class LogicalPlan;

} // namespace litedb::core::logical_planner::plan

namespace litedb::core::logical_planner
{

// CREATE 语句逻辑计划工作器
class LogicalPlannerCreateWorker
{
public:
    LogicalPlannerCreateWorker() = default;

public:
    // 规划 CREATE DATABASE 语句
    [[nodiscard]]
    std::unique_ptr<plan::LogicalPlan> plan_create_database(
        binder::bound::BoundCreateDatabaseStatement & statement
    );

    // 规划 CREATE COLLECTION 语句
    [[nodiscard]]
    std::unique_ptr<plan::LogicalPlan> plan_create_collection(
        binder::bound::BoundCreateCollectionStatement & statement
    );

    // 规划 CREATE INDEX 语句
    [[nodiscard]]
    std::unique_ptr<plan::LogicalPlan> plan_create_index(
        binder::bound::BoundCreateIndexStatement & statement
    );

    // 规划 CREATE VINDEX 语句
    [[nodiscard]]
    std::unique_ptr<plan::LogicalPlan> plan_create_vector_index(
        binder::bound::BoundCreateVectorIndexStatement & statement
    );
};

} // namespace litedb::core::logical_planner

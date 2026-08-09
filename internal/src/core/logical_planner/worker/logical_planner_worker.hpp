#pragma once

#include <memory>

#include "core/binder/bound/dispatcher/statement_dispatcher.hpp"

namespace litedb::core::logical_planner::plan
{

class LogicalPlan;

} // namespace litedb::core::logical_planner::plan

namespace litedb::core::logical_planner
{

// 逻辑计划工作器
class LogicalPlannerWorker
    : private binder::bound::
          MutableBoundStatementDispatcher<LogicalPlannerWorker, std::unique_ptr<plan::LogicalPlan>>
{
    friend binder::bound::
        MutableBoundStatementDispatcher<LogicalPlannerWorker, std::unique_ptr<plan::LogicalPlan>>;

public:
    LogicalPlannerWorker() = default;

public:
    // 规划语句
    [[nodiscard]]
    std::unique_ptr<plan::LogicalPlan> plan_statement(binder::bound::BoundStatement & statement);

private:
    // 访问 CREATE DATABASE 语句
    [[nodiscard]]
    std::unique_ptr<plan::LogicalPlan> visit_create_database_statement(
        binder::bound::BoundCreateDatabaseStatement & statement
    );

    // 访问 CREATE COLLECTION 语句
    [[nodiscard]]
    std::unique_ptr<plan::LogicalPlan> visit_create_collection_statement(
        binder::bound::BoundCreateCollectionStatement & statement
    );

    // 访问 CREATE INDEX 语句
    [[nodiscard]]
    std::unique_ptr<plan::LogicalPlan> visit_create_index_statement(
        binder::bound::BoundCreateIndexStatement & statement
    );

    // 访问 CREATE VINDEX 语句
    [[nodiscard]]
    std::unique_ptr<plan::LogicalPlan> visit_create_vector_index_statement(
        binder::bound::BoundCreateVectorIndexStatement & statement
    );

    // 访问 DELETE 语句
    [[nodiscard]]
    std::unique_ptr<plan::LogicalPlan> visit_delete_statement(
        binder::bound::BoundDeleteStatement & statement
    );

    // 访问 DESCRIBE COLLECTION 语句
    [[nodiscard]]
    std::unique_ptr<plan::LogicalPlan> visit_describe_collection_statement(
        binder::bound::BoundDescribeCollectionStatement & statement
    );

    // 访问 DROP DATABASE 语句
    [[nodiscard]]
    std::unique_ptr<plan::LogicalPlan> visit_drop_database_statement(
        binder::bound::BoundDropDatabaseStatement & statement
    );

    // 访问 DROP COLLECTION 语句
    [[nodiscard]]
    std::unique_ptr<plan::LogicalPlan> visit_drop_collection_statement(
        binder::bound::BoundDropCollectionStatement & statement
    );

    // 访问 DROP INDEX 语句
    [[nodiscard]]
    std::unique_ptr<plan::LogicalPlan> visit_drop_index_statement(
        binder::bound::BoundDropIndexStatement & statement
    );

    // 访问 DROP VINDEX 语句
    [[nodiscard]]
    std::unique_ptr<plan::LogicalPlan> visit_drop_vector_index_statement(
        binder::bound::BoundDropVectorIndexStatement & statement
    );

    // 访问 INSERT 语句
    [[nodiscard]]
    std::unique_ptr<plan::LogicalPlan> visit_insert_statement(
        binder::bound::BoundInsertStatement & statement
    );

    // 访问 SELECT 语句
    [[nodiscard]]
    std::unique_ptr<plan::LogicalPlan> visit_select_statement(
        binder::bound::BoundSelectStatement & statement
    );

    // 访问 SHOW DATABASES 语句
    [[nodiscard]]
    std::unique_ptr<plan::LogicalPlan> visit_show_databases_statement(
        binder::bound::BoundShowDatabasesStatement & statement
    );

    // 访问 SHOW COLLECTIONS 语句
    [[nodiscard]]
    std::unique_ptr<plan::LogicalPlan> visit_show_collections_statement(
        binder::bound::BoundShowCollectionsStatement & statement
    );

    // 访问 SHOW INDEXES 语句
    [[nodiscard]]
    std::unique_ptr<plan::LogicalPlan> visit_show_indexes_statement(
        binder::bound::BoundShowIndexesStatement & statement
    );

    // 访问 SHOW VINDEXES 语句
    [[nodiscard]]
    std::unique_ptr<plan::LogicalPlan> visit_show_vector_indexes_statement(
        binder::bound::BoundShowVectorIndexesStatement & statement
    );

    // 访问 UPDATE 语句
    [[nodiscard]]
    std::unique_ptr<plan::LogicalPlan> visit_update_statement(
        binder::bound::BoundUpdateStatement & statement
    );

    // 访问 USE 语句
    [[nodiscard]]
    std::unique_ptr<plan::LogicalPlan> visit_use_statement(
        binder::bound::BoundUseStatement & statement
    );
};

} // namespace litedb::core::logical_planner

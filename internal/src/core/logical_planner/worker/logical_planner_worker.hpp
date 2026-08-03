#pragma once

#include <expected>
#include <memory>

#include "core/logical_planner/logical_planner_error.hpp"

#include "core/binder/bound/dispatcher/statement_dispatcher.hpp"

namespace litedb::core::logical_planner::plan
{

class LogicalPlan;

} // namespace litedb::core::logical_planner::plan

namespace litedb::core::logical_planner
{

/**
 * @brief 逻辑计划工作器
 */
class LogicalPlannerWorker
    : private binder::bound::BoundStatementDispatcher<
          LogicalPlannerWorker,
          std::expected<std::unique_ptr<plan::LogicalPlan>, LogicalPlannerError>
      >
{
    friend class binder::bound::BoundStatementDispatcher<
        LogicalPlannerWorker,
        std::expected<std::unique_ptr<plan::LogicalPlan>, LogicalPlannerError>
    >;

public:
    LogicalPlannerWorker() = default;

public:
    /**
     * @brief 规划语句
     * @param statement 绑定语句
     * @return 逻辑计划
     */
    [[nodiscard]]
    std::expected<std::unique_ptr<plan::LogicalPlan>, LogicalPlannerError>
    plan_statement(
        binder::bound::BoundStatement & statement
    );

private:
    /**
     * @brief 访问 CREATE DATABASE 语句
     * @param statement CREATE DATABASE 语句
     * @return 逻辑计划
     */
    [[nodiscard]]
    std::expected<std::unique_ptr<plan::LogicalPlan>, LogicalPlannerError>
    visit_create_database_statement(
        const binder::bound::BoundCreateDatabaseStatement & statement
    );

    /**
     * @brief 访问 CREATE COLLECTION 语句
     * @param statement CREATE COLLECTION 语句
     * @return 逻辑计划
     */
    [[nodiscard]]
    std::expected<std::unique_ptr<plan::LogicalPlan>, LogicalPlannerError>
    visit_create_collection_statement(
        const binder::bound::BoundCreateCollectionStatement & statement
    );

    /**
     * @brief 访问 CREATE INDEX 语句
     * @param statement CREATE INDEX 语句
     * @return 逻辑计划
     */
    [[nodiscard]]
    std::expected<std::unique_ptr<plan::LogicalPlan>, LogicalPlannerError>
    visit_create_index_statement(
        const binder::bound::BoundCreateIndexStatement & statement
    );

    /**
     * @brief 访问 CREATE VINDEX 语句
     * @param statement CREATE VINDEX 语句
     * @return 逻辑计划
     */
    [[nodiscard]]
    std::expected<std::unique_ptr<plan::LogicalPlan>, LogicalPlannerError>
    visit_create_vector_index_statement(
        const binder::bound::BoundCreateVectorIndexStatement & statement
    );

    /**
     * @brief 访问 DELETE 语句
     * @param statement DELETE 语句
     * @return 逻辑计划
     */
    [[nodiscard]]
    std::expected<std::unique_ptr<plan::LogicalPlan>, LogicalPlannerError>
    visit_delete_statement(
        binder::bound::BoundDeleteStatement & statement
    );

    /**
     * @brief 访问 DESCRIBE COLLECTION 语句
     * @param statement DESCRIBE COLLECTION 语句
     * @return 逻辑计划
     */
    [[nodiscard]]
    std::expected<std::unique_ptr<plan::LogicalPlan>, LogicalPlannerError>
    visit_describe_collection_statement(
        const binder::bound::BoundDescribeCollectionStatement & statement
    );

    /**
     * @brief 访问 DROP DATABASE 语句
     * @param statement DROP DATABASE 语句
     * @return 逻辑计划
     */
    [[nodiscard]]
    std::expected<std::unique_ptr<plan::LogicalPlan>, LogicalPlannerError>
    visit_drop_database_statement(
        const binder::bound::BoundDropDatabaseStatement & statement
    );

    /**
     * @brief 访问 DROP COLLECTION 语句
     * @param statement DROP COLLECTION 语句
     * @return 逻辑计划
     */
    [[nodiscard]]
    std::expected<std::unique_ptr<plan::LogicalPlan>, LogicalPlannerError>
    visit_drop_collection_statement(
        const binder::bound::BoundDropCollectionStatement & statement
    );

    /**
     * @brief 访问 DROP INDEX 语句
     * @param statement DROP INDEX 语句
     * @return 逻辑计划
     */
    [[nodiscard]]
    std::expected<std::unique_ptr<plan::LogicalPlan>, LogicalPlannerError>
    visit_drop_index_statement(
        const binder::bound::BoundDropIndexStatement & statement
    );

    /**
     * @brief 访问 DROP VINDEX 语句
     * @param statement DROP VINDEX 语句
     * @return 逻辑计划
     */
    [[nodiscard]]
    std::expected<std::unique_ptr<plan::LogicalPlan>, LogicalPlannerError>
    visit_drop_vector_index_statement(
        const binder::bound::BoundDropVectorIndexStatement & statement
    );

    /**
     * @brief 访问 INSERT 语句
     * @param statement INSERT 语句
     * @return 逻辑计划
     * @warning 该成员函数将会移动消费 statement 的成员变量
     */
    [[nodiscard]]
    std::expected<std::unique_ptr<plan::LogicalPlan>, LogicalPlannerError>
    visit_insert_statement(
        binder::bound::BoundInsertStatement & statement
    );

    /**
     * @brief 访问 SELECT 语句
     * @param statement SELECT 语句
     * @return 逻辑计划
     * @warning 该成员函数将会移动消费 statement 的成员变量
     */
    [[nodiscard]]
    std::expected<std::unique_ptr<plan::LogicalPlan>, LogicalPlannerError>
    visit_select_statement(
        binder::bound::BoundSelectStatement & statement
    );

    /**
     * @brief 访问 SHOW DATABASES 语句
     * @param statement SHOW DATABASES 语句
     * @return 逻辑计划
     */
    [[nodiscard]]
    std::expected<std::unique_ptr<plan::LogicalPlan>, LogicalPlannerError>
    visit_show_databases_statement(
        const binder::bound::BoundShowDatabasesStatement & statement
    );

    /**
     * @brief 访问 SHOW COLLECTIONS 语句
     * @param statement SHOW COLLECTIONS 语句
     * @return 逻辑计划
     */
    [[nodiscard]]
    std::expected<std::unique_ptr<plan::LogicalPlan>, LogicalPlannerError>
    visit_show_collections_statement(
        const binder::bound::BoundShowCollectionsStatement & statement
    );

    /**
     * @brief 访问 SHOW INDEXES 语句
     * @param statement SHOW INDEXES 语句
     * @return 逻辑计划
     */
    [[nodiscard]]
    std::expected<std::unique_ptr<plan::LogicalPlan>, LogicalPlannerError>
    visit_show_indexes_statement(
        const binder::bound::BoundShowIndexesStatement & statement
    );

    /**
     * @brief 访问 SHOW VINDEXES 语句
     * @param statement SHOW VINDEXES 语句
     * @return 逻辑计划
     */
    [[nodiscard]]
    std::expected<std::unique_ptr<plan::LogicalPlan>, LogicalPlannerError>
    visit_show_vector_indexes_statement(
        const binder::bound::BoundShowVectorIndexesStatement & statement
    );

    /**
     * @brief 访问 UPDATE 语句
     * @param statement UPDATE 语句
     * @return 逻辑计划
     * @warning 该成员函数将会移动消费 statement 的成员变量
     */
    [[nodiscard]]
    std::expected<std::unique_ptr<plan::LogicalPlan>, LogicalPlannerError>
    visit_update_statement(
        binder::bound::BoundUpdateStatement & statement
    );

    /**
     * @brief 访问 USE 语句
     * @param statement USE 语句
     * @return 逻辑计划
     */
    [[nodiscard]]
    std::expected<std::unique_ptr<plan::LogicalPlan>, LogicalPlannerError>
    visit_use_statement(
        const binder::bound::BoundUseStatement & statement
    );
};

} // namespace litedb::core::logical_planner

#pragma once

#include <cstddef>
#include <iosfwd>
#include <optional>
#include <string>
#include <string_view>

#include "core/logical_planner/plan/dispatcher/logical_plan_dispatcher.hpp"

namespace litedb::core::logical_planner::plan
{

/**
 * @brief 逻辑计划调试打印器
 */
class LogicalPlanDebugPrinter
    : private LogicalPlanDispatcher<LogicalPlanDebugPrinter>
{
    friend class LogicalPlanDispatcher<LogicalPlanDebugPrinter>;

public:
    explicit LogicalPlanDebugPrinter(std::ostream & ostream);

public:
    /**
     * @brief 打印逻辑计划
     * @param plan 逻辑计划
     */
    void print(const LogicalPlan & plan);

private:
    /**
     * @brief 访问 USE 计划
     * @param plan USE 计划
     */
    void visit_use_plan(const UsePlan & plan);

    /**
     * @brief 访问 CREATE DATABASE 计划
     * @param plan CREATE DATABASE 计划
     */
    void visit_create_database_plan(const CreateDatabasePlan & plan);

    /**
     * @brief 访问 CREATE COLLECTION 计划
     * @param plan CREATE COLLECTION 计划
     */
    void visit_create_collection_plan(const CreateCollectionPlan & plan);

    /**
     * @brief 访问 CREATE INDEX 计划
     * @param plan CREATE INDEX 计划
     */
    void visit_create_index_plan(const CreateIndexPlan & plan);

    /**
     * @brief 访问 CREATE VINDEX 计划
     * @param plan CREATE VINDEX 计划
     */
    void visit_create_vector_index_plan(const CreateVectorIndexPlan & plan);

    /**
     * @brief 访问 DROP DATABASE 计划
     * @param plan DROP DATABASE 计划
     */
    void visit_drop_database_plan(const DropDatabasePlan & plan);

    /**
     * @brief 访问 DROP COLLECTION 计划
     * @param plan DROP COLLECTION 计划
     */
    void visit_drop_collection_plan(const DropCollectionPlan & plan);

    /**
     * @brief 访问 DROP INDEX 计划
     * @param plan DROP INDEX 计划
     */
    void visit_drop_index_plan(const DropIndexPlan & plan);

    /**
     * @brief 访问 DROP VINDEX 计划
     * @param plan DROP VINDEX 计划
     */
    void visit_drop_vector_index_plan(const DropVectorIndexPlan & plan);

    /**
     * @brief 访问 SHOW DATABASES 计划
     * @param plan SHOW DATABASES 计划
     */
    void visit_show_databases_plan(const ShowDatabasesPlan & plan);

    /**
     * @brief 访问 SHOW COLLECTIONS 计划
     * @param plan SHOW COLLECTIONS 计划
     */
    void visit_show_collections_plan(const ShowCollectionsPlan & plan);

    /**
     * @brief 访问 SHOW INDEXES 计划
     * @param plan SHOW INDEXES 计划
     */
    void visit_show_indexes_plan(const ShowIndexesPlan & plan);

    /**
     * @brief 访问 SHOW VINDEXES 计划
     * @param plan SHOW VINDEXES 计划
     */
    void visit_show_vector_indexes_plan(const ShowVectorIndexesPlan & plan);

    /**
     * @brief 访问 DESCRIBE COLLECTION 计划
     * @param plan DESCRIBE COLLECTION 计划
     */
    void visit_describe_collection_plan(const DescribeCollectionPlan & plan);

    /**
     * @brief 访问 INSERT 计划
     * @param plan INSERT 计划
     */
    void visit_insert_plan(const InsertPlan & plan);

    /**
     * @brief 访问 UPDATE 计划
     * @param plan UPDATE 计划
     */
    void visit_update_plan(const UpdatePlan & plan);

    /**
     * @brief 访问 DELETE 计划
     * @param plan DELETE 计划
     */
    void visit_delete_plan(const DeletePlan & plan);

    /**
     * @brief 访问 QUERY 计划
     * @param plan QUERY 计划
     */
    void visit_query_plan(const QueryPlan & plan);

private:
    /**
     * @brief 写入缩进
     */
    void write_indent();

    /**
     * @brief 写入节点头
     * @param name 节点名称
     */
    void write_node_header(std::string_view name);

    /**
     * @brief 写入字段
     * @param name 字段名称
     * @param value 字段值
     */
    void write_field(std::string_view name, std::string_view value);

    /**
     * @brief 写入字段
     * @param name 字段名称
     * @param value 字段值
     */
    void write_field(std::string_view name, bool value);

    /**
     * @brief 写入字段
     * @param name 字段名称
     * @param value 字段值
     */
    void write_field(std::string_view name, std::size_t value);

    /**
     * @brief 写入可选字段
     * @param name 字段名称
     * @param value 字段值
     */
    void write_optional_field(
        std::string_view name,
        const std::optional<std::string> & value
    );

    /**
     * @brief 写入可选字段
     * @param name 字段名称
     * @param value 字段值
     */
    void write_optional_field(
        std::string_view name,
        const std::optional<std::size_t> & value
    );

    /**
     * @brief 写入绑定表达式字段
     * @param name 字段名称
     * @param expression 绑定表达式
     */
    void write_expression_field(
        std::string_view name,
        const binder::bound::BoundExpression & expression
    );

    /**
     * @brief 写入子算子字段
     * @param name 字段名称
     * @param child 子算子，允许为空
     */
    void write_operator_field(
        std::string_view name,
        const op::LogicalPlanOperator * child
    );

    /**
     * @brief 缩进作用域
     */
    class IndentScope;

private:
    std::ostream & ostream_;              ///< 输出流
    std::size_t indent_;                  ///< 缩进
    std::string pending_str_;             ///< 待处理的节点前缀
};

/**
 * @brief 调试打印逻辑计划
 * @param plan 逻辑计划
 * @return 调试打印结果
 */
[[nodiscard]]
std::string debug_print(const LogicalPlan & plan);

/**
 * @brief 调试打印逻辑计划
 * @param ostream 输出流
 * @param plan 逻辑计划
 */
void debug_print(
    std::ostream & ostream,
    const LogicalPlan & plan
);

} // namespace litedb::core::logical_planner::plan

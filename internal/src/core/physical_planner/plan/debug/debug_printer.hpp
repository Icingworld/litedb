#pragma once

#include <cstddef>
#include <iosfwd>
#include <optional>
#include <string>
#include <string_view>

#include "core/physical_planner/plan/dispatcher/physical_plan_dispatcher.hpp"

namespace litedb::core::physical_planner::plan
{

// 物理计划调试打印器
class PhysicalPlanDebugPrinter final
    : private ConstPhysicalPlanDispatcher<PhysicalPlanDebugPrinter, void>
{
    friend ConstPhysicalPlanDispatcher<PhysicalPlanDebugPrinter, void>;

public:
    explicit PhysicalPlanDebugPrinter(std::ostream & ostream);

public:
    // 打印物理计划
    void print(const PhysicalPlan & plan);

private:
    // 访问 USE 计划
    void visit_use_plan(const UsePlan & plan);

    // 访问 CREATE DATABASE 计划
    void visit_create_database_plan(const CreateDatabasePlan & plan);

    // 访问 CREATE COLLECTION 计划
    void visit_create_collection_plan(const CreateCollectionPlan & plan);

    // 访问 CREATE INDEX 计划
    void visit_create_index_plan(const CreateIndexPlan & plan);

    // 访问 CREATE VINDEX 计划
    void visit_create_vector_index_plan(const CreateVectorIndexPlan & plan);

    // 访问 DROP DATABASE 计划
    void visit_drop_database_plan(const DropDatabasePlan & plan);

    // 访问 DROP COLLECTION 计划
    void visit_drop_collection_plan(const DropCollectionPlan & plan);

    // 访问 DROP INDEX 计划
    void visit_drop_index_plan(const DropIndexPlan & plan);

    // 访问 DROP VINDEX 计划
    void visit_drop_vector_index_plan(const DropVectorIndexPlan & plan);

    // 访问 SHOW DATABASES 计划
    void visit_show_databases_plan(const ShowDatabasesPlan & plan);

    // 访问 SHOW COLLECTIONS 计划
    void visit_show_collections_plan(const ShowCollectionsPlan & plan);

    // 访问 SHOW INDEXES 计划
    void visit_show_indexes_plan(const ShowIndexesPlan & plan);

    // 访问 SHOW VINDEXES 计划
    void visit_show_vector_indexes_plan(const ShowVectorIndexesPlan & plan);

    // 访问 DESCRIBE COLLECTION 计划
    void visit_describe_collection_plan(const DescribeCollectionPlan & plan);

    // 访问 INSERT 计划
    void visit_insert_plan(const InsertPlan & plan);

    // 访问 UPDATE 计划
    void visit_update_plan(const UpdatePlan & plan);

    // 访问 DELETE 计划
    void visit_delete_plan(const DeletePlan & plan);

    // 访问 QUERY 计划
    void visit_query_plan(const QueryPlan & plan);

private:
    // 写入缩进
    void write_indent();

    // 写入节点头
    void write_node_header(std::string_view name);

    // 写入字段
    void write_field(std::string_view name, std::string_view value);

    // 写入字段
    void write_field(std::string_view name, bool value);

    // 写入字段
    void write_field(std::string_view name, std::size_t value);

    // 写入可选字段
    void write_optional_field(std::string_view name, const std::optional<std::string> & value);

    // 写入可选字段
    void write_optional_field(std::string_view name, const std::optional<std::size_t> & value);

    // 写入绑定表达式字段
    void write_expression_field(
        std::string_view name,
        const binder::bound::BoundExpression & expression
    );

    // 写入物理算子字段
    void write_operator_field(std::string_view name, const op::PhysicalOperator * child);

    // 缩进作用域
    class IndentScope;

private:
    std::ostream & ostream_;
    std::size_t indent_;
    std::string pending_str_;
};

// 调试打印物理计划
[[nodiscard]]
std::string debug_print(const PhysicalPlan & plan);

// 调试打印物理计划
void debug_print(std::ostream & ostream, const PhysicalPlan & plan);

} // namespace litedb::core::physical_planner::plan

#pragma once

#include <cstddef>
#include <iosfwd>
#include <optional>
#include <string>
#include <string_view>

#include "core/physical_planner/plan/dispatcher/physical_plan_dispatcher.hpp"

namespace litedb::core::physical_planner::plan
{

class PhysicalPlanDebugPrinter
    : private ConstPhysicalPlanDispatcher<PhysicalPlanDebugPrinter, void>
{
    friend ConstPhysicalPlanDispatcher<PhysicalPlanDebugPrinter, void>;

public:
    explicit PhysicalPlanDebugPrinter(std::ostream & ostream);
    void print(const PhysicalPlan & plan);

private:
    void visit_use_plan(const UsePlan & plan);
    void visit_create_database_plan(const CreateDatabasePlan & plan);
    void visit_create_collection_plan(const CreateCollectionPlan & plan);
    void visit_create_index_plan(const CreateIndexPlan & plan);
    void visit_create_vector_index_plan(const CreateVectorIndexPlan & plan);
    void visit_drop_database_plan(const DropDatabasePlan & plan);
    void visit_drop_collection_plan(const DropCollectionPlan & plan);
    void visit_drop_index_plan(const DropIndexPlan & plan);
    void visit_drop_vector_index_plan(const DropVectorIndexPlan & plan);
    void visit_show_databases_plan(const ShowDatabasesPlan & plan);
    void visit_show_collections_plan(const ShowCollectionsPlan & plan);
    void visit_show_indexes_plan(const ShowIndexesPlan & plan);
    void visit_show_vector_indexes_plan(const ShowVectorIndexesPlan & plan);
    void visit_describe_collection_plan(const DescribeCollectionPlan & plan);
    void visit_insert_plan(const InsertPlan & plan);
    void visit_update_plan(const UpdatePlan & plan);
    void visit_delete_plan(const DeletePlan & plan);
    void visit_query_plan(const QueryPlan & plan);

    void indent();
    void header(std::string_view name);
    void field(std::string_view name, std::size_t value);
    void field(std::string_view name, std::string_view value);
    void optional_field(std::string_view name, const std::optional<std::string> & value);
    void expression(std::string_view name, const binder::bound::BoundExpression & value);
    void operator_field(std::string_view name, const op::PhysicalOperator & value);

    std::ostream & ostream_;
    std::size_t indent_ {0};
};

[[nodiscard]] std::string debug_print(const PhysicalPlan & plan);
void debug_print(std::ostream & ostream, const PhysicalPlan & plan);

} // namespace litedb::core::physical_planner::plan

#include "core/physical_planner/plan/debug/debug_printer.hpp"

#include <ostream>
#include <optional>
#include <sstream>

#include "core/binder/bound/debug/debug_helper.hpp"
#include "core/binder/bound/debug/debug_printer.hpp"
#include "core/physical_planner/operator/debug/debug_printer.hpp"
#include "core/physical_planner/plan/command/command_plans.hpp"
#include "core/physical_planner/plan/mutation/delete_plan.hpp"
#include "core/physical_planner/plan/mutation/insert_plan.hpp"
#include "core/physical_planner/plan/mutation/update_plan.hpp"
#include "core/physical_planner/plan/query/query_plan.hpp"

namespace litedb::core::physical_planner::plan
{

PhysicalPlanDebugPrinter::PhysicalPlanDebugPrinter(std::ostream & ostream)
    : ostream_(ostream)
{
}

void PhysicalPlanDebugPrinter::print(const PhysicalPlan & plan)
{
    dispatch_plan(plan);
}

void PhysicalPlanDebugPrinter::visit_use_plan(const UsePlan & plan)
{
    header("UsePlan"); ++indent_; field("database_id", plan.database_id()); --indent_;
}

void PhysicalPlanDebugPrinter::visit_create_database_plan(const CreateDatabasePlan & plan)
{
    header("CreateDatabasePlan"); ++indent_; optional_field("database_name", plan.database_name()); --indent_;
}

void PhysicalPlanDebugPrinter::visit_create_collection_plan(const CreateCollectionPlan & plan)
{
    header("CreateCollectionPlan"); ++indent_;
    field("database_id", plan.database_id());
    optional_field("collection_name", plan.collection_name());
    field("column_count", plan.columns().size());
    --indent_;
}

void PhysicalPlanDebugPrinter::visit_create_index_plan(const CreateIndexPlan & plan)
{
    header("CreateIndexPlan"); ++indent_;
    field("column_id", plan.column_id());
    optional_field("index_name", plan.index_name());
    field("index_kind", binder::bound::index_kind_name(plan.index_kind()));
    field("unique", plan.unique() ? "true" : "false");
    --indent_;
}

void PhysicalPlanDebugPrinter::visit_create_vector_index_plan(const CreateVectorIndexPlan & plan)
{
    header("CreateVectorIndexPlan"); ++indent_;
    field("column_id", plan.column_id());
    optional_field("index_name", plan.index_name());
    field("index_kind", binder::bound::vector_index_kind_name(plan.index_kind()));
    field("metric", binder::bound::vector_distance_metric_name(plan.metric()));
    field("max_neighbors", plan.max_neighbors());
    field("ef_construction", plan.ef_construction());
    field("ef_search_default", plan.ef_search_default());
    field("random_seed", plan.random_seed());
    --indent_;
}

void PhysicalPlanDebugPrinter::visit_drop_database_plan(const DropDatabasePlan & plan)
{
    header("DropDatabasePlan"); ++indent_;
    if (plan.database_id()) field("database_id", *plan.database_id()); else field("database_id", "<none>");
    --indent_;
}

void PhysicalPlanDebugPrinter::visit_drop_collection_plan(const DropCollectionPlan & plan)
{
    header("DropCollectionPlan"); ++indent_;
    if (plan.collection_id()) field("collection_id", *plan.collection_id()); else field("collection_id", "<none>");
    --indent_;
}

void PhysicalPlanDebugPrinter::visit_drop_index_plan(const DropIndexPlan & plan)
{
    header("DropIndexPlan"); ++indent_;
    if (plan.index_id()) field("index_id", *plan.index_id()); else field("index_id", "<none>");
    --indent_;
}

void PhysicalPlanDebugPrinter::visit_drop_vector_index_plan(const DropVectorIndexPlan & plan)
{
    header("DropVectorIndexPlan"); ++indent_;
    if (plan.index_id()) field("index_id", *plan.index_id()); else field("index_id", "<none>");
    --indent_;
}

void PhysicalPlanDebugPrinter::visit_show_databases_plan(const ShowDatabasesPlan &)
{
    header("ShowDatabasesPlan");
}

void PhysicalPlanDebugPrinter::visit_show_collections_plan(const ShowCollectionsPlan & plan)
{
    header("ShowCollectionsPlan"); ++indent_; field("database_id", plan.database_id()); --indent_;
}

void PhysicalPlanDebugPrinter::visit_show_indexes_plan(const ShowIndexesPlan & plan)
{
    header("ShowIndexesPlan"); ++indent_; field("collection_id", plan.collection_id()); --indent_;
}

void PhysicalPlanDebugPrinter::visit_show_vector_indexes_plan(const ShowVectorIndexesPlan & plan)
{
    header("ShowVectorIndexesPlan"); ++indent_; field("collection_id", plan.collection_id()); --indent_;
}

void PhysicalPlanDebugPrinter::visit_describe_collection_plan(const DescribeCollectionPlan & plan)
{
    header("DescribeCollectionPlan"); ++indent_; field("collection_id", plan.collection_id()); --indent_;
}

void PhysicalPlanDebugPrinter::visit_insert_plan(const InsertPlan & plan)
{
    header("InsertPlan"); ++indent_; field("collection_id", plan.collection_id()); field("value_count", plan.values().size());
    for (const auto & value : plan.values()) expression("value", *value);
    --indent_;
}

void PhysicalPlanDebugPrinter::visit_update_plan(const UpdatePlan & plan)
{
    header("UpdatePlan"); ++indent_; field("collection_id", plan.collection_id()); field("assignment_count", plan.assignments().size()); operator_field("input", plan.input()); --indent_;
}

void PhysicalPlanDebugPrinter::visit_delete_plan(const DeletePlan & plan)
{
    header("DeletePlan"); ++indent_; field("collection_id", plan.collection_id()); operator_field("input", plan.input()); --indent_;
}

void PhysicalPlanDebugPrinter::visit_query_plan(const QueryPlan & plan)
{
    header("QueryPlan"); ++indent_; operator_field("root", plan.root()); --indent_;
}

void PhysicalPlanDebugPrinter::indent()
{
    for (std::size_t i = 0; i < indent_; ++i) ostream_ << "  ";
}

void PhysicalPlanDebugPrinter::header(std::string_view name)
{
    indent(); ostream_ << name << '\n';
}

void PhysicalPlanDebugPrinter::field(std::string_view name, std::size_t value)
{
    indent(); ostream_ << name << ": " << value << '\n';
}

void PhysicalPlanDebugPrinter::field(std::string_view name, std::string_view value)
{
    indent(); ostream_ << name << ": " << value << '\n';
}

void PhysicalPlanDebugPrinter::optional_field(std::string_view name, const std::optional<std::string> & value)
{
    field(name, value ? std::string_view(*value) : std::string_view("<none>"));
}

void PhysicalPlanDebugPrinter::expression(std::string_view name, const binder::bound::BoundExpression & value)
{
    std::ostringstream stream; binder::bound::debug_print(stream, value);
    field(name, stream.str());
}

void PhysicalPlanDebugPrinter::operator_field(std::string_view name, const op::PhysicalOperator & value)
{
    indent(); ostream_ << name << ":\n"; ++indent_; op::debug_print(ostream_, value); --indent_;
}

std::string debug_print(const PhysicalPlan & plan)
{
    std::ostringstream stream; debug_print(stream, plan); return stream.str();
}

void debug_print(std::ostream & ostream, const PhysicalPlan & plan)
{
    PhysicalPlanDebugPrinter printer {ostream}; printer.print(plan);
}

} // namespace litedb::core::physical_planner::plan

#include "physical_planner_test_support.hpp"

#include <exception>
#include <iostream>
#include <memory>
#include <optional>
#include <sstream>
#include <string>
#include <vector>

#include "core/logical_planner/operator/logical_filter_operator.hpp"
#include "core/logical_planner/operator/logical_limit_operator.hpp"
#include "core/logical_planner/operator/logical_order_by_operator.hpp"
#include "core/logical_planner/operator/logical_projection_operator.hpp"
#include "core/logical_planner/operator/logical_scan_operator.hpp"
#include "core/logical_planner/plan/command/create_collection_plan.hpp"
#include "core/logical_planner/plan/command/create_database_plan.hpp"
#include "core/logical_planner/plan/command/create_index_plan.hpp"
#include "core/logical_planner/plan/command/create_vector_index_plan.hpp"
#include "core/logical_planner/plan/command/describe_collection_plan.hpp"
#include "core/logical_planner/plan/command/drop_collection_plan.hpp"
#include "core/logical_planner/plan/command/drop_database_plan.hpp"
#include "core/logical_planner/plan/command/drop_index_plan.hpp"
#include "core/logical_planner/plan/command/drop_vector_index_plan.hpp"
#include "core/logical_planner/plan/command/show_collections_plan.hpp"
#include "core/logical_planner/plan/command/show_databases_plan.hpp"
#include "core/logical_planner/plan/command/show_indexes_plan.hpp"
#include "core/logical_planner/plan/command/show_vector_indexes_plan.hpp"
#include "core/logical_planner/plan/command/use_plan.hpp"
#include "core/logical_planner/plan/mutation/delete_plan.hpp"
#include "core/logical_planner/plan/mutation/insert_plan.hpp"
#include "core/logical_planner/plan/mutation/update_plan.hpp"
#include "core/logical_planner/plan/query/query_plan.hpp"
#include "core/catalog/catalog_editor.hpp"
#include "core/physical_planner/operator/debug/debug_printer.hpp"
#include "core/physical_planner/operator/physical_filter_operator.hpp"
#include "core/physical_planner/operator/physical_limit_operator.hpp"
#include "core/physical_planner/operator/physical_projection_operator.hpp"
#include "core/physical_planner/operator/physical_seq_scan_operator.hpp"
#include "core/physical_planner/operator/physical_sort_operator.hpp"
#include "core/physical_planner/physical_planner.hpp"
#include "core/physical_planner/plan/command/create_collection_plan.hpp"
#include "core/physical_planner/plan/debug/debug_printer.hpp"
#include "core/physical_planner/plan/mutation/delete_plan.hpp"
#include "core/physical_planner/plan/mutation/insert_plan.hpp"
#include "core/physical_planner/plan/mutation/update_plan.hpp"
#include "core/physical_planner/plan/query/query_plan.hpp"
#include "core/physical_planner/worker/physical_command_worker.hpp"

namespace
{

using namespace litedb::core;
using namespace litedb::core::binder::bound;
using namespace litedb::core::common;
using namespace litedb::core::logical_planner::op;
using namespace litedb::core::logical_planner::plan;
using namespace litedb::core::catalog;
using namespace physical_planner_test_support;

void test_lower_operator_chain()
{
    CatalogEditor catalog;
    physical_planner::PhysicalPlanner planner {catalog.view()};
    std::vector<BoundProjectionItem> projections;
    projections.push_back(
        BoundProjectionItem {.expression = integer_literal(1), .output_name = "constant"}
    );
    std::vector<BoundOrderByItem> order_by;
    order_by.push_back(BoundOrderByItem {.expression = integer_literal(1), .ascending = false});
    auto logical = std::make_unique<QueryPlan>(std::make_unique<LogicalLimitOperator>(
        std::make_unique<LogicalOrderByOperator>(
            std::make_unique<LogicalProjectionOperator>(
                std::make_unique<LogicalFilterOperator>(
                    std::make_unique<LogicalScanOperator>(42),
                    boolean_literal(true)
                ),
                std::move(projections)
            ),
            std::move(order_by)
        ),
        10,
        2
    ));

    auto physical = planner.plan(std::move(logical));
    require(
        physical->kind() == physical_planner::plan::PhysicalPlanKind::Query,
        "query kind mismatch"
    );
    const auto & query = static_cast<const physical_planner::plan::QueryPlan &>(*physical);
    require(
        query.root_operator().kind() == physical_planner::op::PhysicalOperatorKind::Limit,
        "limit lowering mismatch"
    );
    const auto & limit =
        static_cast<const physical_planner::op::LimitOperator &>(query.root_operator());
    require(
        limit.child().kind() == physical_planner::op::PhysicalOperatorKind::Sort,
        "sort lowering mismatch"
    );
    const auto & sort = static_cast<const physical_planner::op::SortOperator &>(limit.child());
    require(
        sort.child().kind() == physical_planner::op::PhysicalOperatorKind::Projection,
        "projection lowering mismatch"
    );
    const auto & projection =
        static_cast<const physical_planner::op::ProjectionOperator &>(sort.child());
    require(
        projection.child().kind() == physical_planner::op::PhysicalOperatorKind::Filter,
        "filter lowering mismatch"
    );
    const auto & filter =
        static_cast<const physical_planner::op::FilterOperator &>(projection.child());
    require(
        filter.child().kind() == physical_planner::op::PhysicalOperatorKind::SeqScan,
        "scan lowering mismatch"
    );

    std::ostringstream printed;
    physical_planner::plan::debug_print(printed, *physical);
    require(
        printed.str().find("collection_id: 42") != std::string::npos,
        "physical debug output missing collection id"
    );
    require(
        printed.str().find("location") == std::string::npos,
        "physical debug output leaked location"
    );
}

void test_all_plan_dispatch_and_ownership()
{
    CatalogEditor catalog;
    physical_planner::PhysicalPlanner planner {catalog.view()};
    auto lower_kind = [&](std::unique_ptr<LogicalPlan> logical) {
        return planner.plan(std::move(logical))->kind();
    };
    require(
        lower_kind(std::make_unique<UsePlan>(1)) == physical_planner::plan::PhysicalPlanKind::Use,
        "USE dispatch"
    );
    require(
        lower_kind(std::make_unique<CreateDatabasePlan>(std::optional<std::string> {"db"})) ==
            physical_planner::plan::PhysicalPlanKind::CreateDatabase,
        "CREATE DATABASE dispatch"
    );
    require(
        lower_kind(
            std::make_unique<CreateCollectionPlan>(
                1,
                std::optional<std::string> {"c"},
                std::vector<ColumnDefinition> {},
                std::nullopt
            )
        ) == physical_planner::plan::PhysicalPlanKind::CreateCollection,
        "CREATE COLLECTION dispatch"
    );
    require(
        lower_kind(
            std::make_unique<CreateIndexPlan>(
                2,
                std::optional<std::string> {"i"},
                catalog::entry::IndexKind::BTree,
                false
            )
        ) == physical_planner::plan::PhysicalPlanKind::CreateIndex,
        "CREATE INDEX dispatch"
    );
    require(
        lower_kind(
            std::make_unique<CreateVectorIndexPlan>(
                2,
                std::optional<std::string> {"v"},
                catalog::entry::VectorIndexKind::Hnsw,
                catalog::entry::VectorDistanceMetric::L2,
                16,
                64,
                32,
                1
            )
        ) == physical_planner::plan::PhysicalPlanKind::CreateVectorIndex,
        "CREATE VECTOR INDEX dispatch"
    );
    require(
        lower_kind(std::make_unique<DropDatabasePlan>(std::optional<DatabaseId> {1})) ==
            physical_planner::plan::PhysicalPlanKind::DropDatabase,
        "DROP DATABASE dispatch"
    );
    require(
        lower_kind(std::make_unique<DropCollectionPlan>(std::optional<CollectionId> {1})) ==
            physical_planner::plan::PhysicalPlanKind::DropCollection,
        "DROP COLLECTION dispatch"
    );
    require(
        lower_kind(std::make_unique<DropIndexPlan>(std::optional<IndexId> {1})) ==
            physical_planner::plan::PhysicalPlanKind::DropIndex,
        "DROP INDEX dispatch"
    );
    require(
        lower_kind(std::make_unique<DropVectorIndexPlan>(std::optional<VIndexId> {1})) ==
            physical_planner::plan::PhysicalPlanKind::DropVectorIndex,
        "DROP VECTOR INDEX dispatch"
    );
    require(
        lower_kind(std::make_unique<ShowDatabasesPlan>()) ==
            physical_planner::plan::PhysicalPlanKind::ShowDatabases,
        "SHOW DATABASES dispatch"
    );
    require(
        lower_kind(std::make_unique<ShowCollectionsPlan>(1)) ==
            physical_planner::plan::PhysicalPlanKind::ShowCollections,
        "SHOW COLLECTIONS dispatch"
    );
    require(
        lower_kind(std::make_unique<ShowIndexesPlan>(1)) ==
            physical_planner::plan::PhysicalPlanKind::ShowIndexes,
        "SHOW INDEXES dispatch"
    );
    require(
        lower_kind(std::make_unique<ShowVectorIndexesPlan>(1)) ==
            physical_planner::plan::PhysicalPlanKind::ShowVectorIndexes,
        "SHOW VECTOR INDEXES dispatch"
    );
    require(
        lower_kind(std::make_unique<DescribeCollectionPlan>(1)) ==
            physical_planner::plan::PhysicalPlanKind::DescribeCollection,
        "DESCRIBE dispatch"
    );

    std::vector<ColumnDefinition> owned_columns {
        ColumnDefinition {
            .name = "id",
            .type = LogicalType {.id = LogicalTypeId::BigInt, .parameter = std::nullopt},
            .nullable = false,
            .comment = "identifier",
        },
    };
    CreateCollectionPlan logical_create_collection {
        1,
        std::optional<std::string> {"owned_collection"},
        std::move(owned_columns),
        std::optional<std::string> {"owned comment"},
    };
    auto physical_create_collection =
        physical_planner::PhysicalCommandWorker {}.plan_create_collection(
            logical_create_collection
        );
    require(
        !logical_create_collection.collection_name().has_value(),
        "physical lowering should consume logical collection name"
    );
    require(
        logical_create_collection.columns().empty(),
        "physical lowering should consume logical columns"
    );
    require(
        !logical_create_collection.comment().has_value(),
        "physical lowering should consume logical collection comment"
    );
    require(
        !logical_create_collection.take_collection_name().has_value(),
        "repeated logical collection name take should be empty after physical lowering"
    );
    require(
        logical_create_collection.take_columns().empty(),
        "repeated logical columns take should be empty after physical lowering"
    );
    require(
        !logical_create_collection.take_comment().has_value(),
        "repeated logical collection comment take should be empty after physical lowering"
    );

    const auto & physical_create_collection_plan =
        static_cast<const physical_planner::plan::CreateCollectionPlan &>(
            *physical_create_collection
        );
    require(
        physical_create_collection_plan.collection_name() == "owned_collection",
        "physical collection name ownership transfer mismatch"
    );
    require(
        physical_create_collection_plan.columns().size() == 1,
        "physical collection columns ownership transfer mismatch"
    );
    require(
        physical_create_collection_plan.columns()[0].comment == "identifier",
        "physical column comment ownership transfer mismatch"
    );
    require(
        physical_create_collection_plan.comment() == "owned comment",
        "physical collection comment ownership transfer mismatch"
    );

    std::vector<std::unique_ptr<BoundExpression>> values;
    values.push_back(integer_literal(1));
    auto insert = planner.plan(std::make_unique<InsertPlan>(1, std::move(values)));
    require(insert->kind() == physical_planner::plan::PhysicalPlanKind::Insert, "INSERT dispatch");
    require(
        static_cast<const physical_planner::plan::InsertPlan &>(*insert).values().size() == 1,
        "INSERT values ownership transfer"
    );

    std::vector<BoundAssignment> assignments;
    assignments.push_back(BoundAssignment {.column_id = 2, .value = integer_literal(2)});
    auto update = planner.plan(
        std::make_unique<UpdatePlan>(
            1,
            std::move(assignments),
            std::make_unique<LogicalScanOperator>(1)
        )
    );
    require(update->kind() == physical_planner::plan::PhysicalPlanKind::Update, "UPDATE dispatch");
    const auto & update_plan = static_cast<const physical_planner::plan::UpdatePlan &>(*update);
    require(update_plan.assignments().size() == 1, "UPDATE assignments ownership transfer");
    require(
        update_plan.root_operator().kind() == physical_planner::op::PhysicalOperatorKind::SeqScan,
        "UPDATE root lowering"
    );

    auto delete_plan =
        planner.plan(std::make_unique<DeletePlan>(1, std::make_unique<LogicalScanOperator>(1)));
    require(
        delete_plan->kind() == physical_planner::plan::PhysicalPlanKind::Delete,
        "DELETE dispatch"
    );
    require(
        static_cast<const physical_planner::plan::DeletePlan &>(*delete_plan)
                .root_operator()
                .kind() == physical_planner::op::PhysicalOperatorKind::SeqScan,
        "DELETE root lowering"
    );
    require(
        lower_kind(std::make_unique<QueryPlan>(std::make_unique<LogicalScanOperator>(1))) ==
            physical_planner::plan::PhysicalPlanKind::Query,
        "QUERY dispatch"
    );
}

} // namespace

int main()
{
    try {
        test_lower_operator_chain();
        test_all_plan_dispatch_and_ownership();
    } catch (const std::exception & error) {
        std::cerr << "physical_planner_tests failed: " << error.what() << '\n';
        return 1;
    }
    std::cout << "physical_planner_tests passed\n";
    return 0;
}

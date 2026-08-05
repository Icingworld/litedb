#include "core/binder/bound/bound_assignment.hpp"
#include "core/binder/bound/bound_order_by_item.hpp"
#include "core/binder/bound/bound_projection_item.hpp"
#include "core/binder/bound/expression/bound_binary_expression.hpp"
#include "core/binder/bound/expression/bound_between_expression.hpp"
#include "core/binder/bound/expression/bound_column_ref_expression.hpp"
#include "core/binder/bound/expression/bound_function_expression.hpp"
#include "core/binder/bound/expression/bound_literal_expression.hpp"
#include "core/common/logical_type.hpp"
#include "core/common/value.hpp"
#include "core/function/builtin/builtin_functions.hpp"
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
#include "core/meta/meta_engine.hpp"
#include "core/physical_planner/operator/debug/debug_printer.hpp"
#include "core/physical_planner/operator/physical_filter_operator.hpp"
#include "core/physical_planner/operator/physical_index_scan_operator.hpp"
#include "core/physical_planner/operator/physical_limit_operator.hpp"
#include "core/physical_planner/operator/physical_projection_operator.hpp"
#include "core/physical_planner/operator/physical_seq_scan_operator.hpp"
#include "core/physical_planner/operator/physical_sort_operator.hpp"
#include "core/physical_planner/operator/physical_vector_search_operator.hpp"
#include "core/physical_planner/physical_planner.hpp"
#include "core/physical_planner/plan/command/command_plans.hpp"
#include "core/physical_planner/plan/debug/debug_printer.hpp"
#include "core/physical_planner/plan/mutation/delete_plan.hpp"
#include "core/physical_planner/plan/mutation/insert_plan.hpp"
#include "core/physical_planner/plan/mutation/update_plan.hpp"
#include "core/physical_planner/plan/query/query_plan.hpp"

#include <cstdint>
#include <exception>
#include <iostream>
#include <limits>
#include <memory>
#include <optional>
#include <sstream>
#include <stdexcept>
#include <string>
#include <string_view>
#include <utility>
#include <vector>

namespace
{

using namespace litedb::core;
using namespace litedb::core::binder::bound;
using namespace litedb::core::common;
using namespace litedb::core::logical_planner::op;
using namespace litedb::core::logical_planner::plan;
using namespace litedb::core::meta;

void require(bool condition, const char * message)
{
    if (!condition) throw std::runtime_error(message);
}

std::unique_ptr<BoundLiteralExpression> literal(LogicalTypeId id, Value value)
{
    return std::make_unique<BoundLiteralExpression>(LogicalType {id}, std::move(value));
}

std::unique_ptr<BoundLiteralExpression> integer_literal(std::int64_t value)
{
    return literal(LogicalTypeId::BigInt, Value {ValueData {value}});
}

std::unique_ptr<BoundLiteralExpression> boolean_literal(bool value)
{
    return literal(LogicalTypeId::Boolean, Value {ValueData {value}});
}

std::unique_ptr<BoundLiteralExpression> small_integer_literal(std::int32_t value)
{
    return literal(LogicalTypeId::Integer, Value {ValueData {value}});
}

std::unique_ptr<BoundColumnRefExpression> column_ref(const meta::entry::ColumnEntry & column)
{
    return std::make_unique<BoundColumnRefExpression>(
        column.id(),
        column.ordinal(),
        column.type()
    );
}

struct PlannerCatalogFixture
{
    CatalogEditor editor;
    DatabaseId database_id {0};
    CollectionId collection_id {0};
    ColumnId age_id {0};
    ColumnId vector_id {0};
    IndexId first_age_index_id {0};
    VIndexId l2_index_id {0};
    VIndexId inner_product_index_id {0};
    VIndexId cosine_index_id {0};
};

PlannerCatalogFixture make_planner_catalog()
{
    CatalogEditor editor;
    auto database = editor.create_database(meta::CreateDatabaseRequest {.name = "planner_db"});
    require(database.has_value(), "planner fixture database creation failed");
    auto collection = editor.create_collection(meta::CreateCollectionRequest {
        .database_id = *database,
        .name = "planner_collection",
        .columns = {
            ColumnDefinition {.name = "id", .type = LogicalType {LogicalTypeId::BigInt, std::nullopt}},
            ColumnDefinition {.name = "age", .type = LogicalType {LogicalTypeId::Integer, std::nullopt}},
            ColumnDefinition {.name = "embedding", .type = LogicalType {LogicalTypeId::Vector, 3}},
        },
    });
    require(collection.has_value(), "planner fixture collection creation failed");

    const auto age = editor.view().find_column(*collection, "age");
    const auto embedding = editor.view().find_column(*collection, "embedding");
    require(age != nullptr && embedding != nullptr, "planner fixture columns missing");

    auto first_age_index = editor.create_index(meta::CreateIndexRequest {
        .collection_id = *collection,
        .column_ids = {age->id()},
        .name = "age_index_first",
    });
    require(first_age_index.has_value(), "planner fixture first scalar index creation failed");
    auto second_age_index = editor.create_index(meta::CreateIndexRequest {
        .collection_id = *collection,
        .column_ids = {age->id()},
        .name = "age_index_second",
    });
    require(second_age_index.has_value(), "planner fixture second scalar index creation failed");

    const auto make_vector_index = [&](std::string name, meta::entry::VectorDistanceMetric metric) {
        return editor.create_vector_index(meta::CreateVectorIndexRequest {
            .collection_id = *collection,
            .column_id = embedding->id(),
            .name = std::move(name),
            .metric = metric,
            .hnsw_options = meta::entry::HnswOptions {
                .max_neighbors = 16,
                .ef_construction = 32,
                .ef_search_default = 16,
                .random_seed = 1,
            },
        });
    };
    auto l2_index = make_vector_index("embedding_l2", meta::entry::VectorDistanceMetric::L2);
    auto inner_product_index = make_vector_index(
        "embedding_inner_product",
        meta::entry::VectorDistanceMetric::InnerProduct
    );
    auto cosine_index = make_vector_index("embedding_cosine", meta::entry::VectorDistanceMetric::Cosine);
    require(l2_index.has_value() && inner_product_index.has_value() && cosine_index.has_value(),
            "planner fixture vector index creation failed");

    return PlannerCatalogFixture {
        .editor = std::move(editor),
        .database_id = *database,
        .collection_id = *collection,
        .age_id = age->id(),
        .vector_id = embedding->id(),
        .first_age_index_id = *first_age_index,
        .l2_index_id = *l2_index,
        .inner_product_index_id = *inner_product_index,
        .cosine_index_id = *cosine_index,
    };
}

std::unique_ptr<BoundExpression> scalar_predicate(
    const meta::entry::ColumnEntry & column,
    BinaryOperator operation,
    std::unique_ptr<BoundExpression> value
)
{
    return std::make_unique<BoundBinaryExpression>(
        column_ref(column),
        operation,
        std::move(value),
        LogicalType {LogicalTypeId::Boolean, std::nullopt}
    );
}

std::unique_ptr<BoundFunctionExpression> vector_distance(
    const meta::entry::ColumnEntry & column,
    std::string_view name,
    std::vector<double> query
)
{
    const auto argument_types = std::vector<LogicalType> {column.type(), column.type()};
    auto function = function::builtin::builtin_function_catalog().bind_scalar(name, argument_types);
    require(function.has_value(), "vector distance function binding failed");

    std::vector<std::unique_ptr<BoundExpression>> arguments;
    arguments.push_back(column_ref(column));
    arguments.push_back(std::make_unique<BoundLiteralExpression>(
        column.type(),
        Value {ValueData {std::move(query)}}
    ));
    return std::make_unique<BoundFunctionExpression>(std::move(*function), std::move(arguments));
}

void test_lower_operator_chain()
{
    CatalogEditor catalog;
    physical_planner::PhysicalPlanner planner {catalog.view()};
    std::vector<BoundProjectionItem> projections;
    projections.push_back(BoundProjectionItem {.expression = integer_literal(1), .output_name = "constant"});
    std::vector<BoundOrderByItem> order_by;
    order_by.push_back(BoundOrderByItem {.expression = integer_literal(1), .ascending = false});
    auto logical = std::make_unique<QueryPlan>(
        std::make_unique<LogicalLimitOperator>(
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
        )
    );

    auto physical = planner.plan(std::move(logical));
    require(physical->kind() == physical_planner::plan::PhysicalPlanKind::Query, "query kind mismatch");
    const auto & query = static_cast<const physical_planner::plan::QueryPlan &>(*physical);
    require(query.root().kind() == physical_planner::op::PhysicalOperatorKind::Limit, "limit lowering mismatch");
    const auto & limit = static_cast<const physical_planner::op::LimitOperator &>(query.root());
    require(limit.child().kind() == physical_planner::op::PhysicalOperatorKind::Sort, "sort lowering mismatch");
    const auto & sort = static_cast<const physical_planner::op::SortOperator &>(limit.child());
    require(sort.child().kind() == physical_planner::op::PhysicalOperatorKind::Projection, "projection lowering mismatch");
    const auto & projection = static_cast<const physical_planner::op::ProjectionOperator &>(sort.child());
    require(projection.child().kind() == physical_planner::op::PhysicalOperatorKind::Filter, "filter lowering mismatch");
    const auto & filter = static_cast<const physical_planner::op::FilterOperator &>(projection.child());
    require(filter.child().kind() == physical_planner::op::PhysicalOperatorKind::SeqScan, "scan lowering mismatch");

    std::ostringstream printed;
    physical_planner::plan::debug_print(printed, *physical);
    require(printed.str().find("collection_id: 42") != std::string::npos, "physical debug output missing collection id");
    require(printed.str().find("location") == std::string::npos, "physical debug output leaked location");
}

void test_scalar_index_selection_and_fallback()
{
    auto fixture = make_planner_catalog();
    const auto * age = fixture.editor.view().find_column(fixture.age_id);
    require(age != nullptr, "scalar planner fixture column missing");
    physical_planner::PhysicalPlanner planner {fixture.editor.view()};

    auto plan_filter = [&](std::unique_ptr<BoundExpression> predicate) {
        return planner.plan(std::make_unique<QueryPlan>(
            std::make_unique<LogicalFilterOperator>(
                std::make_unique<LogicalScanOperator>(fixture.collection_id),
                std::move(predicate)
            )
        ));
    };

    const auto inspect_index = [&](std::unique_ptr<BoundExpression> predicate,
                                   physical_planner::op::IndexLookupKind lookup_kind) {
        auto physical = plan_filter(std::move(predicate));
        const auto & query = static_cast<const physical_planner::plan::QueryPlan &>(*physical);
        const auto & filter = static_cast<const physical_planner::op::FilterOperator &>(query.root());
        const auto & scan = static_cast<const physical_planner::op::IndexScanOperator &>(filter.child());
        require(scan.index_id() == fixture.first_age_index_id, "scalar index selection was not deterministic");
        require(scan.lookup().kind == lookup_kind, "scalar lookup kind mismatch");
        require(filter.predicate().kind() != BoundExpressionKind::Null, "residual predicate was dropped");
        return scan.lookup();
    };

    const auto equal = inspect_index(
        scalar_predicate(*age, BinaryOperator::Equal, small_integer_literal(18)),
        physical_planner::op::IndexLookupKind::Equal
    );
    require(equal.lower.has_value() && !equal.upper.has_value(), "equality lookup bounds mismatch");
    require(std::get<std::int32_t>(equal.lower->key.value().data()) == 18, "equality lookup key mismatch");

    const auto reversed = inspect_index(
        std::make_unique<BoundBinaryExpression>(
            small_integer_literal(18),
            BinaryOperator::LessThan,
            column_ref(*age),
            LogicalType {LogicalTypeId::Boolean, std::nullopt}
        ),
        physical_planner::op::IndexLookupKind::Range
    );
    require(reversed.lower.has_value() && !reversed.lower->inclusive,
            "reversed comparison was not normalized to a lower bound");

    const auto between = inspect_index(
        std::make_unique<BoundBetweenExpression>(
            column_ref(*age),
            small_integer_literal(10),
            small_integer_literal(20)
        ),
        physical_planner::op::IndexLookupKind::Range
    );
    require(between.lower.has_value() && between.upper.has_value(), "BETWEEN bounds missing");
    require(std::get<std::int32_t>(between.lower->key.value().data()) == 10
                && std::get<std::int32_t>(between.upper->key.value().data()) == 20,
            "BETWEEN lookup bounds mismatch");

    auto non_constant = plan_filter(scalar_predicate(*age, BinaryOperator::Equal, column_ref(*age)));
    const auto & non_constant_query = static_cast<const physical_planner::plan::QueryPlan &>(*non_constant);
    const auto & non_constant_filter = static_cast<const physical_planner::op::FilterOperator &>(non_constant_query.root());
    require(non_constant_filter.child().kind() == physical_planner::op::PhysicalOperatorKind::SeqScan,
            "non-constant scalar predicate should fall back to SeqScan");

    auto null_constant = plan_filter(scalar_predicate(
        *age,
        BinaryOperator::Equal,
        literal(LogicalTypeId::Null, Value::null())
    ));
    const auto & null_query = static_cast<const physical_planner::plan::QueryPlan &>(*null_constant);
    const auto & null_filter = static_cast<const physical_planner::op::FilterOperator &>(null_query.root());
    require(null_filter.child().kind() == physical_planner::op::PhysicalOperatorKind::SeqScan,
            "invalid scalar key should fall back to SeqScan");
}

void test_vector_top_k_selection_and_fallback()
{
    auto fixture = make_planner_catalog();
    const auto * id = fixture.editor.view().find_column(fixture.collection_id, "id");
    const auto * embedding = fixture.editor.view().find_column(fixture.vector_id);
    require(id != nullptr && embedding != nullptr, "vector planner fixture columns missing");
    physical_planner::PhysicalPlanner planner {fixture.editor.view()};

    auto make_query = [&](std::string_view function_name,
                          bool ascending,
                          std::optional<std::size_t> limit,
                          std::optional<std::size_t> offset,
                          bool with_filter = true) {
        std::unique_ptr<LogicalPlanOperator> input = std::make_unique<LogicalScanOperator>(fixture.collection_id);
        if (with_filter) {
            input = std::make_unique<LogicalFilterOperator>(std::move(input), boolean_literal(true));
        }
        std::vector<BoundProjectionItem> projections;
        projections.push_back(BoundProjectionItem {.expression = column_ref(*id), .output_name = "id"});
        std::vector<BoundOrderByItem> order_by;
        order_by.push_back(BoundOrderByItem {
            .expression = vector_distance(*embedding, function_name, {1.0, 0.0, 0.0}),
            .ascending = ascending,
        });
        return std::make_unique<QueryPlan>(std::make_unique<LogicalLimitOperator>(
            std::make_unique<LogicalOrderByOperator>(
                std::make_unique<LogicalProjectionOperator>(std::move(input), std::move(projections)),
                std::move(order_by)
            ),
            limit,
            offset
        ));
    };

    const auto inspect_vector = [&](std::string_view function_name,
                                    bool ascending,
                                    VIndexId expected_index,
                                    std::size_t limit,
                                    std::size_t offset) {
        auto physical = planner.plan(make_query(function_name, ascending, limit, offset));
        const auto & query = static_cast<const physical_planner::plan::QueryPlan &>(*physical);
        const auto & root = static_cast<const physical_planner::op::LimitOperator &>(query.root());
        const auto & projection = static_cast<const physical_planner::op::ProjectionOperator &>(root.child());
        const auto & search = static_cast<const physical_planner::op::VectorSearchOperator &>(projection.child());
        require(search.index_id() == expected_index, "vector index selection mismatch");
        require(search.required_count() == limit + offset, "vector required count mismatch");
        require(search.predicate() != nullptr, "vector filter predicate was not preserved");
        require(search.metric() == (function_name == "l2_distance"
                                        ? meta::entry::VectorDistanceMetric::L2
                                        : function_name == "cosine_distance"
                                            ? meta::entry::VectorDistanceMetric::Cosine
                                            : meta::entry::VectorDistanceMetric::InnerProduct),
                "vector metric mismatch");
    };

    inspect_vector("l2_distance", true, fixture.l2_index_id, 2, 3);
    inspect_vector("cosine_distance", true, fixture.cosine_index_id, 3, 1);
    inspect_vector("inner_product", false, fixture.inner_product_index_id, 4, 0);

    auto wrong_direction = planner.plan(make_query("l2_distance", false, 2, 0));
    const auto & wrong_direction_query = static_cast<const physical_planner::plan::QueryPlan &>(*wrong_direction);
    const auto & wrong_direction_root = static_cast<const physical_planner::op::LimitOperator &>(wrong_direction_query.root());
    require(wrong_direction_root.child().kind() == physical_planner::op::PhysicalOperatorKind::Sort,
            "incompatible vector sort direction should fall back to Sort");

    auto overflowing = planner.plan(make_query(
        "l2_distance",
        true,
        std::numeric_limits<std::size_t>::max(),
        1
    ));
    const auto & overflowing_query = static_cast<const physical_planner::plan::QueryPlan &>(*overflowing);
    const auto & overflowing_root = static_cast<const physical_planner::op::LimitOperator &>(overflowing_query.root());
    require(overflowing_root.child().kind() == physical_planner::op::PhysicalOperatorKind::Sort,
            "overflowing vector limit should fall back to Sort");

    CatalogEditor empty_catalog;
    physical_planner::PhysicalPlanner no_index_planner {empty_catalog.view()};
    auto no_index = no_index_planner.plan(make_query("l2_distance", true, 2, 0));
    const auto & no_index_query = static_cast<const physical_planner::plan::QueryPlan &>(*no_index);
    const auto & no_index_root = static_cast<const physical_planner::op::LimitOperator &>(no_index_query.root());
    require(no_index_root.child().kind() == physical_planner::op::PhysicalOperatorKind::Sort,
            "missing vector index should fall back to Sort");
}

void test_all_plan_dispatch()
{
    CatalogEditor catalog;
    physical_planner::PhysicalPlanner planner {catalog.view()};
    const auto collection = std::make_unique<LogicalScanOperator>(7);
    auto lower_kind = [&](std::unique_ptr<LogicalPlan> logical) {
        return planner.plan(std::move(logical))->kind();
    };
    require(lower_kind(std::make_unique<UsePlan>(1)) == physical_planner::plan::PhysicalPlanKind::Use, "USE dispatch");
    require(lower_kind(std::make_unique<CreateDatabasePlan>(std::optional<std::string> {"db"})) == physical_planner::plan::PhysicalPlanKind::CreateDatabase, "CREATE DATABASE dispatch");
    require(lower_kind(std::make_unique<CreateCollectionPlan>(1, std::optional<std::string> {"c"}, std::vector<ColumnDefinition> {}, std::nullopt)) == physical_planner::plan::PhysicalPlanKind::CreateCollection, "CREATE COLLECTION dispatch");
    require(lower_kind(std::make_unique<CreateIndexPlan>(2, std::optional<std::string> {"i"}, meta::entry::IndexKind::BTree, false)) == physical_planner::plan::PhysicalPlanKind::CreateIndex, "CREATE INDEX dispatch");
    require(lower_kind(std::make_unique<CreateVectorIndexPlan>(2, std::optional<std::string> {"v"}, meta::entry::VectorIndexKind::Hnsw, meta::entry::VectorDistanceMetric::L2, 16, 64, 32, 1)) == physical_planner::plan::PhysicalPlanKind::CreateVectorIndex, "CREATE VECTOR INDEX dispatch");
    require(lower_kind(std::make_unique<DropDatabasePlan>(std::optional<DatabaseId> {1})) == physical_planner::plan::PhysicalPlanKind::DropDatabase, "DROP DATABASE dispatch");
    require(lower_kind(std::make_unique<DropCollectionPlan>(std::optional<CollectionId> {1})) == physical_planner::plan::PhysicalPlanKind::DropCollection, "DROP COLLECTION dispatch");
    require(lower_kind(std::make_unique<DropIndexPlan>(std::optional<IndexId> {1})) == physical_planner::plan::PhysicalPlanKind::DropIndex, "DROP INDEX dispatch");
    require(lower_kind(std::make_unique<DropVectorIndexPlan>(std::optional<VIndexId> {1})) == physical_planner::plan::PhysicalPlanKind::DropVectorIndex, "DROP VECTOR INDEX dispatch");
    require(lower_kind(std::make_unique<ShowDatabasesPlan>()) == physical_planner::plan::PhysicalPlanKind::ShowDatabases, "SHOW DATABASES dispatch");
    require(lower_kind(std::make_unique<ShowCollectionsPlan>(1)) == physical_planner::plan::PhysicalPlanKind::ShowCollections, "SHOW COLLECTIONS dispatch");
    require(lower_kind(std::make_unique<ShowIndexesPlan>(1)) == physical_planner::plan::PhysicalPlanKind::ShowIndexes, "SHOW INDEXES dispatch");
    require(lower_kind(std::make_unique<ShowVectorIndexesPlan>(1)) == physical_planner::plan::PhysicalPlanKind::ShowVectorIndexes, "SHOW VECTOR INDEXES dispatch");
    require(lower_kind(std::make_unique<DescribeCollectionPlan>(1)) == physical_planner::plan::PhysicalPlanKind::DescribeCollection, "DESCRIBE dispatch");
    std::vector<std::unique_ptr<BoundExpression>> values;
    values.push_back(integer_literal(1));
    require(lower_kind(std::make_unique<InsertPlan>(1, std::move(values))) == physical_planner::plan::PhysicalPlanKind::Insert, "INSERT dispatch");
    std::vector<BoundAssignment> assignments;
    assignments.push_back(BoundAssignment {.column_id = 2, .value = integer_literal(2)});
    require(lower_kind(std::make_unique<UpdatePlan>(1, std::move(assignments), std::make_unique<LogicalScanOperator>(1))) == physical_planner::plan::PhysicalPlanKind::Update, "UPDATE dispatch");
    require(lower_kind(std::make_unique<DeletePlan>(1, std::make_unique<LogicalScanOperator>(1))) == physical_planner::plan::PhysicalPlanKind::Delete, "DELETE dispatch");
    require(lower_kind(std::make_unique<QueryPlan>(std::make_unique<LogicalScanOperator>(1))) == physical_planner::plan::PhysicalPlanKind::Query, "QUERY dispatch");
    (void) collection;
}

} // namespace

int main()
{
    try {
        test_lower_operator_chain();
        test_scalar_index_selection_and_fallback();
        test_vector_top_k_selection_and_fallback();
        test_all_plan_dispatch();
    } catch (const std::exception & error) {
        std::cerr << "physical_planner_tests failed: " << error.what() << '\n';
        return 1;
    }
    std::cout << "physical_planner_tests passed\n";
    return 0;
}

#include "core/binder/binder.hpp"
#include "core/binder/bound/debug_printer.hpp"
#include "core/meta/meta_engine.hpp"
#include "core/executor/executor.hpp"
#include "core/index/index_engine.hpp"
#include "core/optimizer/optimizer.hpp"
#include "core/parser/ast/statement/statement_node.hpp"
#include "core/parser/parser.hpp"
#include "core/physical_plan/physical_planner.hpp"
#include "core/logical_plan/debug_printer.hpp"
#include "core/logical_plan/node/logical_filter.hpp"
#include "core/logical_plan/node/logical_limit.hpp"
#include "core/logical_plan/node/logical_order_by.hpp"
#include "core/logical_plan/node/logical_plan_node.hpp"
#include "core/logical_plan/node/logical_projection.hpp"
#include "core/logical_plan/node/logical_scan.hpp"
#include "core/logical_plan/node/logical_vector_search.hpp"
#include "core/logical_plan/statement/command/show_databases_plan.hpp"
#include "core/logical_plan/statement/mutation/delete_plan.hpp"
#include "core/logical_plan/statement/mutation/insert_plan.hpp"
#include "core/logical_plan/statement/mutation/update_plan.hpp"
#include "core/logical_plan/statement/query/query_plan.hpp"
#include "core/logical_plan/logical_planner.hpp"
#include "core/storage/schema_loader.hpp"
#include "core/storage/storage_engine.hpp"
#include "core/transaction/transaction_manager.hpp"
#include "core/vindex/vector_index_engine.hpp"
#include "core/wal/wal_store.hpp"
#include "core/filesystem/platform_filesystem.hpp"
#include "../storage/temporary_directory.hpp"

#include <exception>
#include <iostream>
#include <memory>
#include <optional>
#include <stdexcept>
#include <string_view>

namespace
{

using namespace litedb::core::binder;
using namespace litedb::core::binder::bound;
using namespace litedb::core::meta;
using namespace litedb::core::meta::entry;
using namespace litedb::core::common;
using namespace litedb::core::optimizer;
using namespace litedb::core::parser;
using namespace litedb::core::planner;
using namespace litedb::core::planner::logical;
using namespace litedb::core::planner::plan;
using namespace litedb::core::storage;

void require(bool condition, const char * message)
{
    if (!condition) {
        throw std::runtime_error(message);
    }
}

LogicalType type(LogicalTypeId id, std::optional<std::size_t> parameter = std::nullopt)
{
    return LogicalType {id, parameter};
}

struct Fixture
{
    litedb::tests::TemporaryDirectory storage_directory {"litedb-optimizer-tests"};
    litedb::core::filesystem::FileSystem filesystem {litedb::core::filesystem::create_platform_filesystem()};
    CatalogEditor catalog;
    CatalogPublisher publisher {storage_directory.path() / "meta.ldb", filesystem};
    StorageEngine storage {
        storage_directory.path(),
        filesystem,
        litedb::core::storage::StorageOpenMode::TransactionalStaging,
    };
    litedb::core::index::IndexEngine index_engine {storage_directory.path(), filesystem};
    litedb::core::vindex::VectorIndexEngine vector_index_engine {storage_directory.path() / "vindexes", filesystem};
    std::optional<litedb::core::wal::WalManager> wal_store;
    std::unique_ptr<litedb::core::transaction::TransactionManager> transaction_manager;
    DatabaseId database_id {0};
    CollectionId users_id {0};

    Fixture()
    {
        auto database = catalog.create_database(CreateDatabaseRequest {.name = "demo"});
        require(database.has_value(), "fixture database create failed");
        database_id = *database;

        CreateCollectionRequest users;
        users.database_id = database_id;
        users.name = "users";
        users.columns = {
            ColumnDefinition {.name = "id", .type = type(LogicalTypeId::BigInt), .nullable = false},
            ColumnDefinition {.name = "name", .type = type(LogicalTypeId::Varchar, 64)},
            ColumnDefinition {.name = "age", .type = type(LogicalTypeId::Integer), .nullable = true},
            ColumnDefinition {.name = "embedding", .type = type(LogicalTypeId::Vector, 3), .nullable = true},
            ColumnDefinition {.name = "query_embedding", .type = type(LogicalTypeId::Vector, 3), .nullable = true},
        };

        auto collection = catalog.create_collection(users);
        require(collection.has_value(), "fixture collection create failed");
        users_id = *collection;

        auto schema = litedb::core::storage::load_collection_schema(catalog.view(), users_id);
        require(schema.has_value(), "fixture schema load failed");
        auto storage_created = storage.create_collection(std::move(schema.value()));
        require(storage_created.has_value(), "fixture storage create failed");
        auto opened_wal = litedb::core::wal::WalManager::open(storage_directory.path() / "wal", filesystem);
        require(opened_wal.has_value(), "fixture WAL create failed");
        wal_store = std::move(*opened_wal);
        require(publisher.open_or_initialize().has_value(), "fixture catalog publisher open failed");
        require(publisher.publish_committed(catalog.snapshot()).has_value(), "fixture catalog publish failed");
        transaction_manager = std::make_unique<litedb::core::transaction::TransactionManager>(
            storage_directory.path(), filesystem, publisher, storage, index_engine, vector_index_engine, *wal_store, 0
        );
    }
};

std::unique_ptr<litedb::core::parser::ast::StatementNode> parse_ok(std::string_view sql)
{
    Parser parser {std::string(sql)};
    auto result = parser.parse();
    if (!result.has_value()) {
        throw std::runtime_error(result.error().message);
    }
    return std::move(result.value());
}

std::unique_ptr<BoundStatement> bind_ok(Fixture & fixture, std::string_view sql)
{
    auto statement = parse_ok(sql);
    SessionContext session {.current_database_id = fixture.database_id};
    BinderContext context {fixture.catalog.view(), session};
    Binder binder {context};
    auto result = binder.bind(*statement);
    if (!result.has_value()) {
        throw std::runtime_error(result.error().message);
    }
    return std::move(result.value());
}

std::unique_ptr<LogicalStatementPlan> plan_ok(Fixture & fixture, std::string_view sql)
{
    LogicalPlanner planner;
    auto result = planner.plan(bind_ok(fixture, sql));
    if (!result.has_value()) {
        throw std::runtime_error(result.error().message);
    }
    return std::move(result.value());
}

std::unique_ptr<LogicalStatementPlan> optimize_ok(std::unique_ptr<LogicalStatementPlan> plan, OptimizerOptions options = {})
{
    Optimizer optimizer {options};
    auto result = optimizer.optimize(std::move(plan));
    if (!result.has_value()) {
        throw std::runtime_error(result.error().message);
    }
    return std::move(result.value());
}

std::unique_ptr<LogicalStatementPlan> optimize_ok(
    Fixture & fixture,
    std::unique_ptr<LogicalStatementPlan> plan,
    OptimizerOptions options = {}
)
{
    Optimizer optimizer {options, fixture.catalog.view()};
    auto result = optimizer.optimize(std::move(plan));
    if (!result.has_value()) {
        throw std::runtime_error(result.error().message);
    }
    return std::move(result.value());
}

std::expected<litedb::core::executor::ExecutionResult, litedb::core::executor::ExecutionError> execute_plan(
    Fixture & fixture,
    const LogicalStatementPlan & plan
)
{
    litedb::core::executor::Executor executor {
        fixture.catalog.view(), fixture.storage, fixture.index_engine, fixture.vector_index_engine, *fixture.transaction_manager
    };
    litedb::core::physical_plan::PhysicalPlanner physical_planner;
    auto physical = physical_planner.plan(plan);
    return executor.execute(*physical);
}

const LogicalPlanNode & query_root(const LogicalStatementPlan & plan)
{
    require(plan.kind() == LogicalStatementPlanKind::Query, "plan should be query");
    return static_cast<const QueryPlan &>(plan).root();
}

const LogicalFilter & query_filter_child(const LogicalStatementPlan & plan)
{
    const auto & projection = static_cast<const LogicalProjection &>(query_root(plan));
    require(projection.child().kind() == LogicalPlanNodeKind::Filter, "projection child should be filter");
    return static_cast<const LogicalFilter &>(projection.child());
}

void test_passthrough_plans_keep_identity()
{
    Fixture fixture;

    auto show = plan_ok(fixture, "SHOW DATABASES;");
    auto * show_ptr = show.get();
    auto optimized_show = optimize_ok(std::move(show));
    require(optimized_show.get() == show_ptr, "command plans should pass through");
    require(optimized_show->kind() == LogicalStatementPlanKind::ShowDatabases, "SHOW DATABASES kind mismatch");

    auto insert = plan_ok(fixture, "INSERT INTO users (id, name, age) VALUES (1, 'alice', 18);");
    auto * insert_ptr = insert.get();
    auto optimized_insert = optimize_ok(std::move(insert));
    require(optimized_insert.get() == insert_ptr, "insert plans should pass through");
    require(optimized_insert->kind() == LogicalStatementPlanKind::Insert, "INSERT kind mismatch");
}

void test_query_update_delete_are_rebuilt()
{
    Fixture fixture;

    auto query = optimize_ok(plan_ok(fixture, "SELECT id FROM users WHERE true AND age > 18;"));
    require(query->kind() == LogicalStatementPlanKind::Query, "query kind mismatch");
    require(query_filter_child(*query).predicate().kind() == BoundExpressionKind::Binary, "query predicate should simplify");

    auto update = optimize_ok(plan_ok(fixture, "UPDATE users SET age = age + 1 WHERE true;"));
    require(update->kind() == LogicalStatementPlanKind::Update, "update kind mismatch");
    const auto & update_plan = static_cast<const UpdatePlan &>(*update);
    require(update_plan.input().kind() == LogicalPlanNodeKind::Scan, "update Filter(true) should be eliminated");
    require(update_plan.assignments().size() == 1, "update assignments should be cloned");

    auto del = optimize_ok(plan_ok(fixture, "DELETE FROM users WHERE true;"));
    require(del->kind() == LogicalStatementPlanKind::Delete, "delete kind mismatch");
    require(static_cast<const DeletePlan &>(*del).input().kind() == LogicalPlanNodeKind::Scan, "delete Filter(true) should be eliminated");
}

void test_constant_folding()
{
    Fixture fixture;
    auto optimized = optimize_ok(plan_ok(fixture, "SELECT id FROM users WHERE age > 10 + 8;"));
    const auto printed = debug_print(query_root(*optimized));
    require(printed.find("value: 18") != std::string::npos, "constant expression should fold to 18");
    require(printed.find("Plus") == std::string::npos, "folded predicate should not keep Plus");
}

void test_boolean_simplification()
{
    Fixture fixture;

    auto and_plan = optimize_ok(plan_ok(fixture, "SELECT id FROM users WHERE true AND age > 18;"));
    const auto and_printed = litedb::core::binder::bound::debug_print(query_filter_child(*and_plan).predicate());
    require(and_printed.find("And") == std::string::npos, "true AND x should simplify");
    require(and_printed.find("GreaterThan") != std::string::npos, "simplified AND predicate should keep comparison");

    auto or_plan = optimize_ok(plan_ok(fixture, "SELECT id FROM users WHERE false OR age > 18;"));
    const auto or_printed = litedb::core::binder::bound::debug_print(query_filter_child(*or_plan).predicate());
    require(or_printed.find("Or") == std::string::npos, "false OR x should simplify");
    require(or_printed.find("GreaterThan") != std::string::npos, "simplified OR predicate should keep comparison");
}

void test_filter_true_eliminated_and_false_kept()
{
    Fixture fixture;

    auto true_plan = optimize_ok(plan_ok(fixture, "SELECT id FROM users WHERE true;"));
    const auto & true_projection = static_cast<const LogicalProjection &>(query_root(*true_plan));
    require(true_projection.child().kind() == LogicalPlanNodeKind::Scan, "Filter(true) should be eliminated");

    auto false_plan = optimize_ok(plan_ok(fixture, "SELECT id FROM users WHERE false;"));
    const auto & false_projection = static_cast<const LogicalProjection &>(query_root(*false_plan));
    require(false_projection.child().kind() == LogicalPlanNodeKind::Filter, "Filter(false) should be kept");
}

void test_clone_debug_print_equivalence()
{
    Fixture fixture;
    auto plan = plan_ok(fixture, "SELECT id, name FROM users WHERE age > 10 ORDER BY name;");
    const auto before = debug_print(query_root(*plan));
    auto cloned = query_root(*plan).clone();
    const auto after = debug_print(*cloned);
    require(before == after, "logical clone should preserve debug print");
}

void test_disabled_optimizer_preserves_plan_shape()
{
    Fixture fixture;
    OptimizerOptions disabled;
    disabled.enabled = false;
    auto optimized = optimize_ok(plan_ok(fixture, "SELECT id FROM users WHERE true;"), disabled);
    const auto & projection = static_cast<const LogicalProjection &>(query_root(*optimized));
    require(projection.child().kind() == LogicalPlanNodeKind::Filter, "disabled optimizer should preserve Filter(true)");
}

void test_enabled_and_disabled_select_results_match()
{
    Fixture fixture;

    auto insert = plan_ok(fixture, "INSERT INTO users (id, name, age) VALUES (1, 'alice', 18);");
    auto inserted = execute_plan(fixture, *insert);
    require(inserted.has_value(), "fixture insert should execute");

    OptimizerOptions disabled;
    disabled.enabled = false;
    auto disabled_plan = optimize_ok(plan_ok(fixture, "SELECT id FROM users WHERE true AND age > 10 + 7;"), disabled);
    auto enabled_plan = optimize_ok(plan_ok(fixture, "SELECT id FROM users WHERE true AND age > 10 + 7;"));

    auto disabled_result = execute_plan(fixture, *disabled_plan);
    auto enabled_result = execute_plan(fixture, *enabled_plan);
    require(disabled_result.has_value(), "disabled optimizer query should execute");
    require(enabled_result.has_value(), "enabled optimizer query should execute");
    require(disabled_result->rows.size() == enabled_result->rows.size(), "row count should match");
    require(disabled_result->rows.size() == 1, "query should return one row");
    require(disabled_result->rows[0].values.size() == enabled_result->rows[0].values.size(), "value count should match");
    require(
        disabled_result->rows[0].values[0].data() == enabled_result->rows[0].values[0].data(),
        "enabled and disabled optimizer results should match"
    );
}

void create_catalog_index(Fixture & fixture, std::string name, std::string_view column_name, IndexKind kind)
{
    const auto * column = fixture.catalog.view().find_column(fixture.users_id, column_name);
    require(column != nullptr, "fixture index column missing");
    auto created = fixture.catalog.create_index(CreateIndexRequest {
        .collection_id = fixture.users_id,
        .column_ids = {column->id()},
        .name = std::move(name),
        .kind = kind,
    });
    require(created.has_value(), "fixture catalog index create failed");
}

const LogicalPlanNode & filter_child_for_query(const LogicalStatementPlan & plan)
{
    return query_filter_child(plan).child();
}

void test_btree_equality_adds_scan_index_hint()
{
    Fixture fixture;
    create_catalog_index(fixture, "idx_age_btree", "age", IndexKind::BTree);

    auto optimized = optimize_ok(fixture, plan_ok(fixture, "SELECT id FROM users WHERE age = 18;"));
    const auto & child = filter_child_for_query(*optimized);
    require(child.kind() == LogicalPlanNodeKind::Scan, "BTREE equality should keep LogicalScan");

    const auto & scan = static_cast<const LogicalScan &>(child);
    require(scan.index_hint().has_value(), "BTREE equality should add scan index hint");
    require(scan.index_hint()->index_name == "idx_age_btree", "index name mismatch");
    require(scan.index_hint()->column_name == "age", "index column mismatch");
    require(scan.index_hint()->lookup.kind == LogicalIndexLookupKind::Equal, "equality lookup kind mismatch");
}

void test_btree_range_adds_scan_index_hint()
{
    Fixture fixture;
    create_catalog_index(fixture, "idx_age_btree", "age", IndexKind::BTree);

    auto optimized = optimize_ok(fixture, plan_ok(fixture, "SELECT id FROM users WHERE age >= 18;"));
    const auto & child = filter_child_for_query(*optimized);
    require(child.kind() == LogicalPlanNodeKind::Scan, "BTREE range should keep LogicalScan");

    const auto & scan = static_cast<const LogicalScan &>(child);
    require(scan.index_hint().has_value(), "BTREE range should add scan index hint");
    require(scan.index_hint()->lookup.kind == LogicalIndexLookupKind::Range, "range lookup kind mismatch");
    require(scan.index_hint()->lookup.lower.has_value(), "range lower bound should exist");
    require(scan.index_hint()->lookup.lower->inclusive, "range lower bound should be inclusive");
}

VIndexId create_catalog_vector_index(
    Fixture & fixture,
    std::string name,
    VectorDistanceMetric metric
)
{
    const auto * column = fixture.catalog.view().find_column(fixture.users_id, "embedding");
    require(column != nullptr, "fixture vector column missing");
    auto created = fixture.catalog.create_vector_index(CreateVectorIndexRequest {
        .collection_id = fixture.users_id,
        .column_id = column->id(),
        .name = std::move(name),
        .kind = VectorIndexKind::Hnsw,
        .metric = metric,
    });
    require(created.has_value(), "fixture vector index create failed");
    return *created;
}

const LogicalVectorSearch * find_vector_search(const LogicalStatementPlan & plan)
{
    const auto * node = &query_root(plan);
    if (node->kind() != LogicalPlanNodeKind::Limit) return nullptr;
    node = &static_cast<const LogicalLimit *>(node)->child();
    if (node->kind() != LogicalPlanNodeKind::OrderBy) return nullptr;
    node = &static_cast<const LogicalOrderBy *>(node)->child();
    if (node->kind() != LogicalPlanNodeKind::Projection) return nullptr;
    node = &static_cast<const LogicalProjection *>(node)->child();
    if (node->kind() != LogicalPlanNodeKind::VectorSearch) return nullptr;
    return static_cast<const LogicalVectorSearch *>(node);
}

void test_vector_topk_rewrite_metrics_where_offset_and_stable_index()
{
    Fixture fixture;
    const auto first_l2 = create_catalog_vector_index(fixture, "vidx_l2_first", VectorDistanceMetric::L2);
    (void) create_catalog_vector_index(fixture, "vidx_l2_second", VectorDistanceMetric::L2);
    const auto cosine = create_catalog_vector_index(fixture, "vidx_cos", VectorDistanceMetric::Cosine);
    const auto inner_product = create_catalog_vector_index(fixture, "vidx_ip", VectorDistanceMetric::InnerProduct);

    auto l2 = optimize_ok(
        fixture,
        plan_ok(fixture, "SELECT id FROM users WHERE age >= 18 ORDER BY l2_distance([0, 0, 0], embedding) ASC LIMIT 2 OFFSET 3;")
    );
    const auto * l2_search = find_vector_search(*l2);
    require(l2_search != nullptr, "eligible L2 TopK should rewrite to vector search");
    require(l2_search->index_id() == first_l2, "multiple matching indexes should select the smallest id");
    require(l2_search->required_count() == 5, "vector search should include OFFSET in required count");
    require(l2_search->predicate() != nullptr, "WHERE predicate should be absorbed into vector search");
    auto cloned_search = l2_search->clone();
    const auto & cloned_vector_search = static_cast<const LogicalVectorSearch &>(*cloned_search);
    require(cloned_vector_search.index_id() == first_l2, "vector search clone should preserve index id");
    require(cloned_vector_search.required_count() == 5, "vector search clone should preserve required count");
    require(cloned_vector_search.predicate() != nullptr, "vector search clone should preserve predicate");
    const auto printed = debug_print(query_root(*l2));
    require(printed.find("LogicalVectorSearch") != std::string::npos, "debug printer should include vector search node");
    require(printed.find("metric: L2") != std::string::npos, "debug printer should include vector metric");

    auto cosine_plan = optimize_ok(
        fixture,
        plan_ok(fixture, "SELECT id FROM users ORDER BY cosine_distance(embedding, [1, 0, 0]) ASC LIMIT 1;")
    );
    require(find_vector_search(*cosine_plan)->index_id() == cosine, "Cosine ASC should select cosine index");

    auto ip_plan = optimize_ok(
        fixture,
        plan_ok(fixture, "SELECT id FROM users ORDER BY inner_product(embedding, [1, 0, 0]) DESC LIMIT 1;")
    );
    require(find_vector_search(*ip_plan)->index_id() == inner_product, "InnerProduct DESC should select IP index");
}

void test_vector_topk_rewrite_rejects_ineligible_queries()
{
    Fixture fixture;
    (void) create_catalog_vector_index(fixture, "vidx_l2", VectorDistanceMetric::L2);

    const auto unchanged = [&](std::string_view sql, const char * message) {
        auto plan = optimize_ok(fixture, plan_ok(fixture, sql));
        require(find_vector_search(*plan) == nullptr, message);
    };
    unchanged(
        "SELECT id FROM users ORDER BY l2_distance(embedding, [0, 0, 0]) DESC LIMIT 1;",
        "wrong L2 direction must not rewrite"
    );
    unchanged(
        "SELECT id FROM users ORDER BY l2_distance(embedding, [0, 0, 0]) ASC, id ASC LIMIT 1;",
        "multiple sort keys must not rewrite"
    );
    unchanged(
        "SELECT id FROM users ORDER BY l2_distance(embedding, query_embedding) ASC LIMIT 1;",
        "non-constant query vector must not rewrite"
    );
    unchanged(
        "SELECT id FROM users ORDER BY l2_distance(embedding, [0, 0, 0]) ASC;",
        "vector ordering without LIMIT must not rewrite"
    );

    Fixture no_index;
    auto no_index_plan = optimize_ok(
        no_index,
        plan_ok(no_index, "SELECT id FROM users ORDER BY l2_distance(embedding, [0, 0, 0]) ASC LIMIT 1;")
    );
    require(find_vector_search(*no_index_plan) == nullptr, "query without matching vector index must not rewrite");
}

void test_vector_search_reports_missing_runtime_index()
{
    Fixture fixture;
    (void) create_catalog_vector_index(fixture, "vidx_l2", VectorDistanceMetric::L2);
    auto plan = optimize_ok(
        fixture,
        plan_ok(fixture, "SELECT id FROM users ORDER BY l2_distance(embedding, [0, 0, 0]) ASC LIMIT 1;")
    );
    auto executed = execute_plan(fixture, *plan);
    require(!executed.has_value(), "missing runtime vector index should fail execution");
    require(
        executed.error().code == litedb::core::executor::ExecutionErrorCode::IndexError,
        "missing runtime vector index should report IndexError"
    );
}

} // namespace

int main()
{
    try {
        test_passthrough_plans_keep_identity();
        test_query_update_delete_are_rebuilt();
        test_constant_folding();
        test_boolean_simplification();
        test_filter_true_eliminated_and_false_kept();
        test_clone_debug_print_equivalence();
        test_disabled_optimizer_preserves_plan_shape();
        test_enabled_and_disabled_select_results_match();
        test_btree_equality_adds_scan_index_hint();
        test_btree_range_adds_scan_index_hint();
        test_vector_topk_rewrite_metrics_where_offset_and_stable_index();
        test_vector_topk_rewrite_rejects_ineligible_queries();
        test_vector_search_reports_missing_runtime_index();
    } catch (const std::exception & exception) {
        std::cerr << exception.what() << '\n';
        return 1;
    }

    return 0;
}

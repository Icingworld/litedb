#include "core/binder/binder.hpp"
#include "core/meta/meta_engine.hpp"
#include "core/parser/ast/statement/statement_node.hpp"
#include "core/parser/parser.hpp"
#include "core/index/index_engine.hpp"
#include "core/logical_planner/logical_planner.hpp"
#include "core/logical_planner/operator/debug/debug_printer.hpp"
#include "core/logical_planner/operator/logical_filter_operator.hpp"
#include "core/logical_planner/operator/logical_limit_operator.hpp"
#include "core/logical_planner/operator/logical_order_by_operator.hpp"
#include "core/logical_planner/operator/logical_plan_operator.hpp"
#include "core/logical_planner/operator/logical_projection_operator.hpp"
#include "core/logical_planner/operator/logical_scan_operator.hpp"
#include "core/logical_planner/plan/command/create_collection_plan.hpp"
#include "core/logical_planner/plan/command/create_database_plan.hpp"
#include "core/logical_planner/plan/command/create_index_plan.hpp"
#include "core/logical_planner/plan/command/create_vector_index_plan.hpp"
#include "core/logical_planner/plan/mutation/delete_plan.hpp"
#include "core/logical_planner/plan/command/describe_collection_plan.hpp"
#include "core/logical_planner/plan/command/drop_collection_plan.hpp"
#include "core/logical_planner/plan/command/drop_database_plan.hpp"
#include "core/logical_planner/plan/command/drop_index_plan.hpp"
#include "core/logical_planner/plan/command/drop_vector_index_plan.hpp"
#include "core/logical_planner/plan/mutation/insert_plan.hpp"
#include "core/logical_planner/plan/query/query_plan.hpp"
#include "core/logical_planner/plan/command/show_collections_plan.hpp"
#include "core/logical_planner/plan/command/show_indexes_plan.hpp"
#include "core/logical_planner/plan/command/show_vector_indexes_plan.hpp"
#include "core/logical_planner/plan/debug/debug_printer.hpp"
#include "core/logical_planner/plan/logical_plan.hpp"
#include "core/logical_planner/plan/mutation/update_plan.hpp"
#include "core/logical_planner/plan/command/use_plan.hpp"
#include "core/storage/schema_loader.hpp"
#include "core/storage/storage_engine.hpp"
#include "core/filesystem/platform_filesystem.hpp"
#include "../storage/temporary_directory.hpp"

#include <exception>
#include <iostream>
#include <memory>
#include <optional>
#include <stdexcept>
#include <string>
#include <string_view>

namespace
{

using namespace litedb::core::binder;
using namespace litedb::core::binder::bound;
using namespace litedb::core::meta;
using namespace litedb::core::meta::entry;
using namespace litedb::core::common;
using namespace litedb::core::index;
using namespace litedb::core::parser;
using namespace litedb::core::logical_planner;
using namespace litedb::core::logical_planner::op;
using namespace litedb::core::logical_planner::plan;
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

std::unique_ptr<litedb::core::parser::ast::StatementNode> parse_ok(std::string_view sql)
{
    Parser parser {std::string(sql)};
    auto result = parser.parse();
    if (!result.has_value()) {
        throw std::runtime_error(std::string(result.error().message()).append(": ").append(sql));
    }
    return std::move(*result);
}

struct Fixture
{
    litedb::tests::TemporaryDirectory storage_directory {"litedb-logical-planner-tests"};
    litedb::core::filesystem::FileSystem filesystem {litedb::core::filesystem::create_platform_filesystem()};
    CatalogEditor catalog;
    StorageEngine storage {
        storage_directory.path(),
        filesystem,
        litedb::core::storage::StorageOpenMode::TransactionalStaging,
    };
    IndexEngine index_engine {storage_directory.path(), filesystem};
    DatabaseId database_id {0};
    CollectionId users_id {0};

    Fixture()
    {
        auto database = catalog.create_database(CreateDatabaseRequest {.name = "demo"});
        if (!database.has_value()) {
            throw std::runtime_error(database.error().message());
        }
        database_id = *database;

        CreateCollectionRequest users;
        users.database_id = database_id;
        users.name = "users";
        users.columns = {
            ColumnDefinition {
                .name = "id",
                .type = type(LogicalTypeId::BigInt),
            },
            ColumnDefinition {
                .name = "name",
                .type = type(LogicalTypeId::Varchar, 64),
                .default_expression = litedb::core::schema::DefaultExpression::literal(litedb::core::schema::DefaultLiteralKind::String, "unknown"),
            },
            ColumnDefinition {
                .name = "age",
                .type = type(LogicalTypeId::Integer),
                .nullable = true,
            },
            ColumnDefinition {
                .name = "embedding",
                .type = type(LogicalTypeId::Vector, 3),
                .nullable = true,
            },
        };

        auto collection = catalog.create_collection(users);
        if (!collection.has_value()) {
            throw std::runtime_error(collection.error().message());
        }
        users_id = *collection;

        auto schema = litedb::core::storage::load_collection_schema(catalog.view(), users_id);
        if (!schema.has_value()) {
            throw std::runtime_error(schema.error().message());
        }
        auto storage_created = storage.create_collection(std::move(*schema));
        if (!storage_created.has_value()) {
            throw std::runtime_error(storage_created.error().message());
        }
    }
};

std::unique_ptr<BoundStatement> bind_ok(Fixture & fixture, std::string_view sql)
{
    auto statement = parse_ok(sql);
    SessionContext session {.current_database_id = fixture.database_id};
    BinderContext context {fixture.catalog.view(), session};
    Binder binder {context};
    auto result = binder.bind(*statement);
    if (!result.has_value()) {
        throw std::runtime_error(result.error().message());
    }
    return std::move(*result);
}

std::unique_ptr<LogicalPlan> plan_ok(Fixture & fixture, std::string_view sql)
{
    auto statement = bind_ok(fixture, sql);
    LogicalPlanner planner;
    auto result = planner.plan(*statement);
    if (!result.has_value()) {
        throw std::runtime_error(result.error().message());
    }
    return std::move(*result);
}

const LogicalPlanOperator & query_root(const LogicalPlan & plan)
{
    require(plan.kind() == LogicalPlanKind::Query, "plan should be query");
    return static_cast<const QueryPlan &>(plan).root_operator();
}

IndexId create_managed_index(
    Fixture & fixture,
    std::string name,
    std::string_view column_name,
    litedb::core::meta::entry::IndexKind kind
)
{
    const auto * column = fixture.catalog.view().find_column(fixture.users_id, std::string(column_name));
    require(column != nullptr, "fixture index column missing");

    auto created = fixture.catalog.create_index(CreateIndexRequest {
        .collection_id = fixture.users_id,
        .column_ids = {column->id()},
        .name = std::move(name),
        .kind = kind,
    });
    if (!created.has_value()) {
        throw std::runtime_error(std::string {created.error().message()});
    }

    const auto * entry = fixture.catalog.view().find_index(*created);
    require(entry != nullptr, "fixture index entry missing");

    auto schema = litedb::core::storage::load_collection_schema(fixture.catalog.view(), fixture.users_id);
    if (!schema.has_value()) {
        throw std::runtime_error(schema.error().message());
    }
    require(fixture.storage.contains_collection(fixture.users_id), "fixture collection storage missing");
    auto managed = fixture.index_engine.create_index(*entry, *schema, fixture.storage);
    if (!managed.has_value()) {
        throw std::runtime_error(managed.error().message());
    }
    return *created;
}

void test_select_full_chain()
{
    Fixture fixture;
    auto plan = plan_ok(
        fixture,
        "SELECT * FROM users WHERE age >= 18 ORDER BY id DESC LIMIT 10 OFFSET 5;"
    );

    const auto & root = query_root(*plan);
    require(root.kind() == LogicalPlanOperatorKind::Limit, "SELECT root should be limit");
    const auto & limit = static_cast<const LogicalLimitOperator &>(root);
    require(limit.limit().value() == 10, "SELECT limit mismatch");
    require(limit.offset().value() == 5, "SELECT offset mismatch");

    require(limit.child().kind() == LogicalPlanOperatorKind::OrderBy, "SELECT child should be order by");
    const auto & order_by = static_cast<const LogicalOrderByOperator &>(limit.child());
    require(order_by.order_by().size() == 1, "SELECT order count mismatch");
    require(!order_by.order_by()[0].ascending, "SELECT order direction mismatch");

    require(order_by.child().kind() == LogicalPlanOperatorKind::Projection, "SELECT order child should be projection");
    const auto & projection = static_cast<const LogicalProjectionOperator &>(order_by.child());
    require(projection.projections().size() == 4, "SELECT wildcard projection count mismatch");

    require(projection.child().kind() == LogicalPlanOperatorKind::Filter, "SELECT projection child should be filter");
    const auto & filter = static_cast<const LogicalFilterOperator &>(projection.child());
    require(filter.predicate().type().id == LogicalTypeId::Boolean, "SELECT filter predicate type mismatch");

    require(filter.child().kind() == LogicalPlanOperatorKind::Scan, "SELECT filter child should be scan");
    const auto & scan = static_cast<const LogicalScanOperator &>(filter.child());
    require(scan.collection_id() == fixture.users_id, "SELECT scan collection id mismatch");
}

void test_select_minimal_chain()
{
    Fixture fixture;
    auto plan = plan_ok(fixture, "SELECT id, name FROM users;");

    const auto & root = query_root(*plan);
    require(root.kind() == LogicalPlanOperatorKind::Projection, "minimal SELECT root should be projection");
    const auto & projection = static_cast<const LogicalProjectionOperator &>(root);
    require(projection.projections().size() == 2, "minimal SELECT projection count mismatch");
    require(projection.child().kind() == LogicalPlanOperatorKind::Scan, "minimal SELECT child should be scan");

    const auto operator_text = op::debug_print(root);
    require(operator_text.find("LogicalProjectionOperator") != std::string::npos, "operator debug print should include projection");
    require(operator_text.find("LogicalScanOperator") != std::string::npos, "operator debug print should include scan");

    const auto plan_text = plan::debug_print(*plan);
    require(plan_text.find("QueryPlan") != std::string::npos, "plan debug print should include query");
    require(plan_text.find("LogicalProjectionOperator") != std::string::npos, "plan debug print should include root operator");
}

void test_insert_plan()
{
    Fixture fixture;
    auto plan = plan_ok(fixture, "INSERT INTO users (id, age, embedding) VALUES (1, 18, [0.1, 0.2, 0.3]);");

    require(plan->kind() == LogicalPlanKind::Insert, "INSERT kind mismatch");
    const auto & insert = static_cast<const InsertPlan &>(*plan);
    require(insert.collection_id() == fixture.users_id, "INSERT collection id mismatch");
    require(insert.values().size() == 4, "INSERT values count mismatch");
}

void test_update_delete_plans()
{
    Fixture fixture;
    auto update = plan_ok(fixture, "UPDATE users SET age = age + 1 WHERE id = 1;");
    require(update->kind() == LogicalPlanKind::Update, "UPDATE kind mismatch");
    const auto & update_node = static_cast<const UpdatePlan &>(*update);
    require(update_node.collection_id() == fixture.users_id, "UPDATE collection id mismatch");
    require(update_node.assignments().size() == 1, "UPDATE assignment count mismatch");
    require(update_node.root_operator().kind() == LogicalPlanOperatorKind::Filter, "UPDATE with WHERE should have filter input");
    const auto & update_filter = static_cast<const LogicalFilterOperator &>(update_node.root_operator());
    require(update_filter.child().kind() == LogicalPlanOperatorKind::Scan, "UPDATE filter child should be scan");

    auto update_without_where = plan_ok(fixture, "UPDATE users SET age = 20;");
    require(update_without_where->kind() == LogicalPlanKind::Update, "UPDATE without WHERE kind mismatch");
    const auto & update_without_where_node = static_cast<const UpdatePlan &>(*update_without_where);
    require(update_without_where_node.root_operator().kind() == LogicalPlanOperatorKind::Scan, "UPDATE without WHERE should scan directly");

    auto del = plan_ok(fixture, "DELETE FROM users WHERE id = 1;");
    require(del->kind() == LogicalPlanKind::Delete, "DELETE kind mismatch");
    const auto & delete_node = static_cast<const DeletePlan &>(*del);
    require(delete_node.collection_id() == fixture.users_id, "DELETE collection id mismatch");
    require(delete_node.root_operator().kind() == LogicalPlanOperatorKind::Filter, "DELETE with WHERE should have filter input");

    auto delete_without_where = plan_ok(fixture, "DELETE FROM users;");
    require(delete_without_where->kind() == LogicalPlanKind::Delete, "DELETE without WHERE kind mismatch");
    const auto & delete_without_where_node = static_cast<const DeletePlan &>(*delete_without_where);
    require(delete_without_where_node.root_operator().kind() == LogicalPlanOperatorKind::Scan, "DELETE without WHERE should scan directly");
}

void test_indexes_do_not_change_logical_scan()
{
    Fixture fixture;
    (void) create_managed_index(fixture, "idx_age_btree", "age", litedb::core::meta::entry::IndexKind::BTree);

    auto equal = plan_ok(fixture, "SELECT id FROM users WHERE age = 18;");
    const auto & equal_projection = static_cast<const LogicalProjectionOperator &>(query_root(*equal));
    const auto & equal_filter = static_cast<const LogicalFilterOperator &>(equal_projection.child());
    require(equal_filter.child().kind() == LogicalPlanOperatorKind::Scan, "equality should remain logical scan");

    auto range = plan_ok(fixture, "SELECT id FROM users WHERE age >= 18;");
    const auto & range_projection = static_cast<const LogicalProjectionOperator &>(query_root(*range));
    const auto & range_filter = static_cast<const LogicalFilterOperator &>(range_projection.child());
    require(range_filter.child().kind() == LogicalPlanOperatorKind::Scan, "range should remain logical scan");

    auto between = plan_ok(fixture, "SELECT id FROM users WHERE age BETWEEN 18 AND 30;");
    const auto & between_projection = static_cast<const LogicalProjectionOperator &>(query_root(*between));
    const auto & between_filter = static_cast<const LogicalFilterOperator &>(between_projection.child());
    require(between_filter.child().kind() == LogicalPlanOperatorKind::Scan, "between should remain logical scan");

    auto fallback_like = plan_ok(fixture, "SELECT id FROM users WHERE name LIKE 'a%';");
    const auto & like_projection = static_cast<const LogicalProjectionOperator &>(query_root(*fallback_like));
    const auto & like_filter = static_cast<const LogicalFilterOperator &>(like_projection.child());
    require(like_filter.child().kind() == LogicalPlanOperatorKind::Scan, "LIKE should fall back to scan");

    auto fallback_expression = plan_ok(fixture, "SELECT id FROM users WHERE age + 1 = 19;");
    const auto & expr_projection = static_cast<const LogicalProjectionOperator &>(query_root(*fallback_expression));
    const auto & expr_filter = static_cast<const LogicalFilterOperator &>(expr_projection.child());
    require(expr_filter.child().kind() == LogicalPlanOperatorKind::Scan, "expression predicate should fall back to scan");
}

void test_admin_and_ddl_plans()
{
    Fixture fixture;
    const auto * age_column = fixture.catalog.view().find_column(
        fixture.users_id,
        "age"
    );
    require(age_column != nullptr, "age column lookup failed");
    const auto * embedding_column = fixture.catalog.view().find_column(
        fixture.users_id,
        "embedding"
    );
    require(embedding_column != nullptr, "embedding column lookup failed");

    auto use = plan_ok(fixture, "USE demo;");
    require(use->kind() == LogicalPlanKind::Use, "USE kind mismatch");
    require(static_cast<const UsePlan &>(*use).database_id() == fixture.database_id, "USE database id mismatch");

    auto create_database = plan_ok(fixture, "CREATE DATABASE demo2;");
    require(create_database->kind() == LogicalPlanKind::CreateDatabase, "CREATE DATABASE kind mismatch");
    require(static_cast<const CreateDatabasePlan &>(*create_database).database_name() == "demo2", "CREATE DATABASE name mismatch");

    auto create_collection = plan_ok(fixture, "CREATE COLLECTION posts (id BIGINT, embedding VECTOR(3));");
    require(create_collection->kind() == LogicalPlanKind::CreateCollection, "CREATE COLLECTION kind mismatch");
    const auto & create_collection_node = static_cast<const CreateCollectionPlan &>(*create_collection);
    require(create_collection_node.database_id() == fixture.database_id, "CREATE COLLECTION database id mismatch");
    require(create_collection_node.columns().size() == 2, "CREATE COLLECTION column count mismatch");

    auto create_index = plan_ok(fixture, "CREATE INDEX IF NOT EXISTS idx_age ON users (age) USING BTREE;");
    require(create_index->kind() == LogicalPlanKind::CreateIndex, "CREATE INDEX kind mismatch");
    const auto & create_index_node = static_cast<const CreateIndexPlan &>(*create_index);
    require(create_index_node.column_id() == age_column->id(), "CREATE INDEX column id mismatch");
    require(create_index_node.index_name() == "idx_age", "CREATE INDEX index name mismatch");
    require(create_index_node.index_kind() == litedb::core::meta::entry::IndexKind::BTree, "CREATE INDEX kind value mismatch");
    require(!create_index_node.unique(), "CREATE INDEX unique mismatch");

    auto create_vector_index = plan_ok(
        fixture,
        "CREATE VINDEX IF NOT EXISTS vidx_embedding ON users (embedding) USING HNSW "
        "WITH (metric = COSINE, max_neighbors = 24, ef_construction = 240, ef_search = 80, random_seed = 7);"
    );
    require(create_vector_index->kind() == LogicalPlanKind::CreateVectorIndex, "CREATE VINDEX kind mismatch");
    const auto & create_vector_index_node = static_cast<const CreateVectorIndexPlan &>(*create_vector_index);
    require(create_vector_index_node.column_id() == embedding_column->id(), "CREATE VINDEX column id mismatch");
    require(create_vector_index_node.vector_index_name() == "vidx_embedding", "CREATE VINDEX index name mismatch");
    require(create_vector_index_node.vector_index_kind() == VectorIndexKind::Hnsw, "CREATE VINDEX kind value mismatch");
    require(create_vector_index_node.metric() == VectorDistanceMetric::Cosine, "CREATE VINDEX metric mismatch");
    require(create_vector_index_node.max_neighbors() == 24, "CREATE VINDEX max_neighbors mismatch");
    require(create_vector_index_node.ef_construction() == 240, "CREATE VINDEX ef_construction mismatch");
    require(create_vector_index_node.ef_search_default() == 80, "CREATE VINDEX ef_search mismatch");
    require(create_vector_index_node.random_seed() == 7, "CREATE VINDEX random_seed mismatch");

    auto drop_database = plan_ok(fixture, "DROP DATABASE IF EXISTS missing;");
    require(drop_database->kind() == LogicalPlanKind::DropDatabase, "DROP DATABASE kind mismatch");
    const auto & drop_database_node = static_cast<const DropDatabasePlan &>(*drop_database);
    require(!drop_database_node.database_id().has_value(), "missing database id should be empty");

    auto drop_collection = plan_ok(fixture, "DROP COLLECTION IF EXISTS missing;");
    require(drop_collection->kind() == LogicalPlanKind::DropCollection, "DROP COLLECTION kind mismatch");
    const auto & drop_collection_node = static_cast<const DropCollectionPlan &>(*drop_collection);
    require(!drop_collection_node.collection_id().has_value(), "missing collection id should be empty");

    auto created_index = fixture.catalog.create_index(CreateIndexRequest {
        .collection_id = fixture.users_id,
        .column_ids = {age_column->id()},
        .name = "idx_age",
        .kind = litedb::core::meta::entry::IndexKind::BTree,
    });
    require(created_index.has_value(), "fixture index create failed");

    auto drop_index = plan_ok(fixture, "DROP INDEX idx_age ON users;");
    require(drop_index->kind() == LogicalPlanKind::DropIndex, "DROP INDEX kind mismatch");
    const auto & drop_index_node = static_cast<const DropIndexPlan &>(*drop_index);
    require(drop_index_node.index_id() == *created_index, "DROP INDEX id mismatch");

    auto created_vector_index = fixture.catalog.create_vector_index(CreateVectorIndexRequest {
        .collection_id = fixture.users_id,
        .column_id = embedding_column->id(),
        .name = "vidx_embedding",
    });
    require(created_vector_index.has_value(), "fixture vector index create failed");

    auto drop_vector_index = plan_ok(fixture, "DROP VINDEX vidx_embedding ON users;");
    require(drop_vector_index->kind() == LogicalPlanKind::DropVectorIndex, "DROP VINDEX kind mismatch");
    const auto & drop_vector_index_node = static_cast<const DropVectorIndexPlan &>(*drop_vector_index);
    require(drop_vector_index_node.vector_index_id() == *created_vector_index, "DROP VINDEX id mismatch");

    require(plan_ok(fixture, "SHOW DATABASES;")->kind() == LogicalPlanKind::ShowDatabases, "SHOW DATABASES kind mismatch");

    auto show_collections = plan_ok(fixture, "SHOW COLLECTIONS;");
    require(show_collections->kind() == LogicalPlanKind::ShowCollections, "SHOW COLLECTIONS kind mismatch");
    require(static_cast<const ShowCollectionsPlan &>(*show_collections).database_id() == fixture.database_id, "SHOW COLLECTIONS database id mismatch");

    auto show_indexes = plan_ok(fixture, "SHOW INDEXES FROM users;");
    require(show_indexes->kind() == LogicalPlanKind::ShowIndexes, "SHOW INDEXES kind mismatch");
    const auto & show_indexes_node = static_cast<const ShowIndexesPlan &>(*show_indexes);
    require(show_indexes_node.collection_id() == fixture.users_id, "SHOW INDEXES collection id mismatch");

    auto show_vector_indexes = plan_ok(fixture, "SHOW VINDEXES FROM users;");
    require(show_vector_indexes->kind() == LogicalPlanKind::ShowVectorIndexes, "SHOW VINDEXES kind mismatch");
    const auto & show_vector_indexes_node = static_cast<const ShowVectorIndexesPlan &>(*show_vector_indexes);
    require(show_vector_indexes_node.collection_id() == fixture.users_id, "SHOW VINDEXES collection id mismatch");

    auto describe = plan_ok(fixture, "DESCRIBE users;");
    require(describe->kind() == LogicalPlanKind::DescribeCollection, "DESCRIBE kind mismatch");
    const auto & describe_node = static_cast<const DescribeCollectionPlan &>(*describe);
    require(describe_node.collection_id() == fixture.users_id, "DESCRIBE collection id mismatch");
}

void test_ownership_transfer_interfaces()
{
    Fixture fixture;

    auto query = plan_ok(
        fixture,
        "SELECT id, name FROM users WHERE age >= 18 ORDER BY id DESC LIMIT 10;"
    );
    auto & query_plan = static_cast<QueryPlan &>(*query);
    auto root = query_plan.take_root_operator();
    require(root != nullptr, "query root ownership transfer failed");
    require(root->kind() == LogicalPlanOperatorKind::Limit, "transferred query root should be limit");

    auto order_by_node = static_cast<LogicalLimitOperator &>(*root).take_child();
    require(order_by_node != nullptr, "limit child ownership transfer failed");
    auto & order_by = static_cast<LogicalOrderByOperator &>(*order_by_node);
    auto order_by_items = order_by.take_order_by();
    require(order_by_items.size() == 1, "order by ownership transfer count mismatch");
    require(order_by_items[0].expression != nullptr, "order by expression ownership transfer failed");

    auto projection_node = order_by.take_child();
    require(projection_node != nullptr, "order by child ownership transfer failed");
    auto & projection = static_cast<LogicalProjectionOperator &>(*projection_node);
    auto projections = projection.take_projections();
    require(projections.size() == 2, "projection ownership transfer count mismatch");
    require(projections[0].expression != nullptr, "projection expression ownership transfer failed");

    auto filter_node = projection.take_child();
    require(filter_node != nullptr, "projection child ownership transfer failed");
    auto & filter = static_cast<LogicalFilterOperator &>(*filter_node);
    auto predicate = filter.take_predicate();
    require(predicate != nullptr, "predicate ownership transfer failed");
    auto scan = filter.take_child();
    require(scan != nullptr, "filter child ownership transfer failed");
    require(scan->kind() == LogicalPlanOperatorKind::Scan, "transferred filter child should be scan");

    auto update = plan_ok(fixture, "UPDATE users SET age = 21 WHERE id = 1;");
    auto & update_plan = static_cast<UpdatePlan &>(*update);
    auto assignments = update_plan.take_assignments();
    require(assignments.size() == 1, "update assignment ownership transfer failed");
    require(assignments[0].value != nullptr, "update assignment expression ownership transfer failed");
    require(update_plan.take_root_operator() != nullptr, "update root ownership transfer failed");

    auto delete_statement = plan_ok(fixture, "DELETE FROM users WHERE id = 1;");
    auto & delete_plan = static_cast<DeletePlan &>(*delete_statement);
    require(delete_plan.take_root_operator() != nullptr, "delete root ownership transfer failed");

    auto insert = plan_ok(fixture, "INSERT INTO users VALUES (1, 'Alice', 20, [1.0, 2.0, 3.0]);");
    auto values = static_cast<InsertPlan &>(*insert).take_values();
    require(values.size() == 4, "insert values ownership transfer failed");
    require(values[0] != nullptr, "insert expression ownership transfer failed");

}

} // namespace

int main()
{
    try {
        test_select_full_chain();
        test_select_minimal_chain();
        test_insert_plan();
        test_update_delete_plans();
        test_indexes_do_not_change_logical_scan();
        test_admin_and_ddl_plans();
        test_ownership_transfer_interfaces();
    } catch (const std::exception & exception) {
        std::cerr << exception.what() << '\n';
        return 1;
    }

    return 0;
}

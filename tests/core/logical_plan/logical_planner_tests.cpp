#include "core/binder/binder.hpp"
#include "core/meta/meta_engine.hpp"
#include "core/parser/ast/statement/statement_node.hpp"
#include "core/parser/parser.hpp"
#include "core/index/index_engine.hpp"
#include "core/logical_plan/logical_planner.hpp"
#include "core/logical_plan/debug_printer.hpp"
#include "core/logical_plan/node/logical_filter.hpp"
#include "core/logical_plan/node/logical_limit.hpp"
#include "core/logical_plan/node/logical_order_by.hpp"
#include "core/logical_plan/node/logical_plan_node.hpp"
#include "core/logical_plan/logical_planner.hpp"
#include "core/logical_plan/node/logical_projection.hpp"
#include "core/logical_plan/node/logical_scan.hpp"
#include "core/logical_plan/statement/command/create_collection_plan.hpp"
#include "core/logical_plan/statement/command/create_database_plan.hpp"
#include "core/logical_plan/statement/command/create_index_plan.hpp"
#include "core/logical_plan/statement/command/create_vector_index_plan.hpp"
#include "core/logical_plan/statement/mutation/delete_plan.hpp"
#include "core/logical_plan/statement/command/describe_collection_plan.hpp"
#include "core/logical_plan/statement/command/drop_collection_plan.hpp"
#include "core/logical_plan/statement/command/drop_database_plan.hpp"
#include "core/logical_plan/statement/command/drop_index_plan.hpp"
#include "core/logical_plan/statement/command/drop_vector_index_plan.hpp"
#include "core/logical_plan/statement/mutation/insert_plan.hpp"
#include "core/logical_plan/statement/query/query_plan.hpp"
#include "core/logical_plan/statement/command/show_collections_plan.hpp"
#include "core/logical_plan/statement/command/show_indexes_plan.hpp"
#include "core/logical_plan/statement/command/show_vector_indexes_plan.hpp"
#include "core/logical_plan/statement/logical_statement_plan.hpp"
#include "core/logical_plan/statement/mutation/update_plan.hpp"
#include "core/logical_plan/statement/command/use_plan.hpp"
#include "core/schema/schema_loader.hpp"
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
using namespace litedb::core::planner;
using namespace litedb::core::planner::plan;
using namespace litedb::core::planner::logical;
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
        throw std::runtime_error(std::string(result.error().message).append(": ").append(sql));
    }
    return std::move(result.value());
}

struct Fixture
{
    litedb::tests::TemporaryDirectory storage_directory {"litedb-logical-planner-tests"};
    litedb::core::filesystem::FileSystem filesystem {litedb::core::filesystem::create_platform_filesystem()};
    MetaEngine catalog;
    StorageEngine storage {storage_directory.path(), filesystem};
    IndexEngine index_engine {storage_directory.path(), filesystem};
    DatabaseId database_id {0};
    CollectionId users_id {0};

    Fixture()
    {
        auto database = catalog.create_database(CreateDatabaseRequest {.name = "demo"});
        if (!database.has_value()) {
            throw std::runtime_error(database.error().message);
        }
        database_id = database.value();

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
                .default_expression = DefaultExpression::literal(DefaultLiteralKind::String, "unknown"),
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
            throw std::runtime_error(collection.error().message);
        }
        users_id = collection.value();

        auto schema = litedb::core::schema::load_collection_schema(catalog, users_id);
        if (!schema.has_value()) {
            throw std::runtime_error(schema.error().message);
        }
        auto storage_created = storage.create_collection(std::move(schema.value()));
        if (!storage_created.has_value()) {
            throw std::runtime_error(storage_created.error().message);
        }
    }
};

std::unique_ptr<BoundStatement> bind_ok(Fixture & fixture, std::string_view sql)
{
    auto statement = parse_ok(sql);
    SessionContext session {.current_database_id = fixture.database_id};
    BinderContext context {fixture.catalog, session};
    Binder binder {context};
    auto result = binder.bind(*statement);
    if (!result.has_value()) {
        throw std::runtime_error(result.error().message);
    }
    return std::move(result.value());
}

std::unique_ptr<LogicalStatementPlan> plan_ok(
    Fixture & fixture,
    std::string_view sql,
    const IndexEngine * index_engine = nullptr
)
{
    (void) index_engine;
    LogicalPlanner planner;
    auto result = planner.plan(bind_ok(fixture, sql));
    if (!result.has_value()) {
        throw std::runtime_error(result.error().message);
    }
    return std::move(result.value());
}

const LogicalPlanNode & query_root(const LogicalStatementPlan & plan)
{
    require(plan.kind() == LogicalStatementPlanKind::Query, "plan should be query");
    return static_cast<const QueryPlan &>(plan).root();
}

IndexId create_managed_index(
    Fixture & fixture,
    std::string name,
    std::string_view column_name,
    litedb::core::meta::entry::IndexKind kind
)
{
    const auto * column = fixture.catalog.find_column(fixture.users_id, std::string(column_name));
    require(column != nullptr, "fixture index column missing");

    auto created = fixture.catalog.create_index(CreateIndexRequest {
        .collection_id = fixture.users_id,
        .column_ids = {column->id()},
        .name = std::move(name),
        .kind = kind,
    });
    if (!created.has_value()) {
        throw std::runtime_error(created.error().message);
    }

    const auto * entry = fixture.catalog.find_index(created.value());
    require(entry != nullptr, "fixture index entry missing");

    auto schema = litedb::core::schema::load_collection_schema(fixture.catalog, fixture.users_id);
    if (!schema.has_value()) {
        throw std::runtime_error(schema.error().message);
    }
    require(fixture.storage.contains_collection(fixture.users_id), "fixture collection storage missing");
    auto managed = fixture.index_engine.create_index(*entry, schema.value(), fixture.storage);
    if (!managed.has_value()) {
        throw std::runtime_error(managed.error().message);
    }
    return created.value();
}

void test_select_full_chain()
{
    Fixture fixture;
    auto plan = plan_ok(
        fixture,
        "SELECT * FROM users WHERE age >= 18 ORDER BY id DESC LIMIT 10 OFFSET 5;"
    );

    const auto & root = query_root(*plan);
    require(root.kind() == LogicalPlanNodeKind::Limit, "SELECT root should be limit");
    const auto & limit = static_cast<const LogicalLimit &>(root);
    require(limit.limit().value() == 10, "SELECT limit mismatch");
    require(limit.offset().value() == 5, "SELECT offset mismatch");

    require(limit.child().kind() == LogicalPlanNodeKind::OrderBy, "SELECT child should be order by");
    const auto & order_by = static_cast<const LogicalOrderBy &>(limit.child());
    require(order_by.order_by().size() == 1, "SELECT order count mismatch");
    require(!order_by.order_by()[0].ascending, "SELECT order direction mismatch");

    require(order_by.child().kind() == LogicalPlanNodeKind::Projection, "SELECT order child should be projection");
    const auto & projection = static_cast<const LogicalProjection &>(order_by.child());
    require(projection.projections().size() == 4, "SELECT wildcard projection count mismatch");

    require(projection.child().kind() == LogicalPlanNodeKind::Filter, "SELECT projection child should be filter");
    const auto & filter = static_cast<const LogicalFilter &>(projection.child());
    require(filter.predicate().type().id == LogicalTypeId::Boolean, "SELECT filter predicate type mismatch");

    require(filter.child().kind() == LogicalPlanNodeKind::Scan, "SELECT filter child should be scan");
    const auto & scan = static_cast<const LogicalScan &>(filter.child());
    require(scan.database_id() == fixture.database_id, "SELECT scan database id mismatch");
    require(scan.collection_id() == fixture.users_id, "SELECT scan collection id mismatch");
    require(scan.collection_name() == "users", "SELECT scan collection name mismatch");
}

void test_select_minimal_chain()
{
    Fixture fixture;
    auto plan = plan_ok(fixture, "SELECT id, name FROM users;");

    const auto & root = query_root(*plan);
    require(root.kind() == LogicalPlanNodeKind::Projection, "minimal SELECT root should be projection");
    const auto & projection = static_cast<const LogicalProjection &>(root);
    require(projection.projections().size() == 2, "minimal SELECT projection count mismatch");
    require(projection.child().kind() == LogicalPlanNodeKind::Scan, "minimal SELECT child should be scan");

    const auto printed = debug_print(root);
    require(printed.find("LogicalProjection") != std::string::npos, "debug print should include projection");
    require(printed.find("LogicalScan") != std::string::npos, "debug print should include scan");
}

void test_insert_plan()
{
    Fixture fixture;
    auto plan = plan_ok(fixture, "INSERT INTO users (id, age, embedding) VALUES (1, 18, [0.1, 0.2, 0.3]);");

    require(plan->kind() == LogicalStatementPlanKind::Insert, "INSERT kind mismatch");
    const auto & insert = static_cast<const InsertPlan &>(*plan);
    require(insert.database_id() == fixture.database_id, "INSERT database id mismatch");
    require(insert.collection_id() == fixture.users_id, "INSERT collection id mismatch");
    require(insert.columns().size() == 4, "INSERT columns count mismatch");
    require(insert.values().size() == 4, "INSERT values count mismatch");
}

void test_update_delete_plans()
{
    Fixture fixture;
    auto update = plan_ok(fixture, "UPDATE users SET age = age + 1 WHERE id = 1;");
    require(update->kind() == LogicalStatementPlanKind::Update, "UPDATE kind mismatch");
    const auto & update_node = static_cast<const UpdatePlan &>(*update);
    require(update_node.assignments().size() == 1, "UPDATE assignment count mismatch");
    require(update_node.input().kind() == LogicalPlanNodeKind::Filter, "UPDATE with WHERE should have filter input");
    const auto & update_filter = static_cast<const LogicalFilter &>(update_node.input());
    require(update_filter.child().kind() == LogicalPlanNodeKind::Scan, "UPDATE filter child should be scan");

    auto update_without_where = plan_ok(fixture, "UPDATE users SET age = 20;");
    require(update_without_where->kind() == LogicalStatementPlanKind::Update, "UPDATE without WHERE kind mismatch");
    const auto & update_without_where_node = static_cast<const UpdatePlan &>(*update_without_where);
    require(update_without_where_node.input().kind() == LogicalPlanNodeKind::Scan, "UPDATE without WHERE should scan directly");

    auto del = plan_ok(fixture, "DELETE FROM users WHERE id = 1;");
    require(del->kind() == LogicalStatementPlanKind::Delete, "DELETE kind mismatch");
    const auto & delete_node = static_cast<const DeletePlan &>(*del);
    require(delete_node.input().kind() == LogicalPlanNodeKind::Filter, "DELETE with WHERE should have filter input");

    auto delete_without_where = plan_ok(fixture, "DELETE FROM users;");
    require(delete_without_where->kind() == LogicalStatementPlanKind::Delete, "DELETE without WHERE kind mismatch");
    const auto & delete_without_where_node = static_cast<const DeletePlan &>(*delete_without_where);
    require(delete_without_where_node.input().kind() == LogicalPlanNodeKind::Scan, "DELETE without WHERE should scan directly");
}

void test_indexes_do_not_change_logical_scan()
{
    Fixture fixture;
    (void) create_managed_index(fixture, "idx_age_btree", "age", litedb::core::meta::entry::IndexKind::BTree);

    auto equal = plan_ok(fixture, "SELECT id FROM users WHERE age = 18;", &fixture.index_engine);
    const auto & equal_projection = static_cast<const LogicalProjection &>(query_root(*equal));
    const auto & equal_filter = static_cast<const LogicalFilter &>(equal_projection.child());
    require(equal_filter.child().kind() == LogicalPlanNodeKind::Scan, "equality should remain logical scan");

    auto range = plan_ok(fixture, "SELECT id FROM users WHERE age >= 18;", &fixture.index_engine);
    const auto & range_projection = static_cast<const LogicalProjection &>(query_root(*range));
    const auto & range_filter = static_cast<const LogicalFilter &>(range_projection.child());
    require(range_filter.child().kind() == LogicalPlanNodeKind::Scan, "range should remain logical scan");

    auto between = plan_ok(fixture, "SELECT id FROM users WHERE age BETWEEN 18 AND 30;", &fixture.index_engine);
    const auto & between_projection = static_cast<const LogicalProjection &>(query_root(*between));
    const auto & between_filter = static_cast<const LogicalFilter &>(between_projection.child());
    require(between_filter.child().kind() == LogicalPlanNodeKind::Scan, "between should remain logical scan");

    auto fallback_like = plan_ok(fixture, "SELECT id FROM users WHERE name LIKE 'a%';", &fixture.index_engine);
    const auto & like_projection = static_cast<const LogicalProjection &>(query_root(*fallback_like));
    const auto & like_filter = static_cast<const LogicalFilter &>(like_projection.child());
    require(like_filter.child().kind() == LogicalPlanNodeKind::Scan, "LIKE should fall back to scan");

    auto fallback_expression = plan_ok(fixture, "SELECT id FROM users WHERE age + 1 = 19;", &fixture.index_engine);
    const auto & expr_projection = static_cast<const LogicalProjection &>(query_root(*fallback_expression));
    const auto & expr_filter = static_cast<const LogicalFilter &>(expr_projection.child());
    require(expr_filter.child().kind() == LogicalPlanNodeKind::Scan, "expression predicate should fall back to scan");
}

void test_admin_and_ddl_plans()
{
    Fixture fixture;

    auto use = plan_ok(fixture, "USE demo;");
    require(use->kind() == LogicalStatementPlanKind::Use, "USE kind mismatch");
    require(static_cast<const UsePlan &>(*use).database_id() == fixture.database_id, "USE database id mismatch");

    auto create_database = plan_ok(fixture, "CREATE DATABASE demo2;");
    require(create_database->kind() == LogicalStatementPlanKind::CreateDatabase, "CREATE DATABASE kind mismatch");
    require(static_cast<const CreateDatabasePlan &>(*create_database).database_name() == "demo2", "CREATE DATABASE name mismatch");

    auto create_collection = plan_ok(fixture, "CREATE COLLECTION posts (id BIGINT, embedding VECTOR(3));");
    require(create_collection->kind() == LogicalStatementPlanKind::CreateCollection, "CREATE COLLECTION kind mismatch");
    const auto & create_collection_node = static_cast<const CreateCollectionPlan &>(*create_collection);
    require(create_collection_node.database_id() == fixture.database_id, "CREATE COLLECTION database id mismatch");
    require(create_collection_node.columns().size() == 2, "CREATE COLLECTION column count mismatch");

    auto create_index = plan_ok(fixture, "CREATE INDEX IF NOT EXISTS idx_age ON users (age) USING BTREE;");
    require(create_index->kind() == LogicalStatementPlanKind::CreateIndex, "CREATE INDEX kind mismatch");
    const auto & create_index_node = static_cast<const CreateIndexPlan &>(*create_index);
    require(create_index_node.database_id() == fixture.database_id, "CREATE INDEX database id mismatch");
    require(create_index_node.collection_id() == fixture.users_id, "CREATE INDEX collection id mismatch");
    require(create_index_node.collection_name() == "users", "CREATE INDEX collection name mismatch");
    require(create_index_node.column_name() == "age", "CREATE INDEX column name mismatch");
    require(create_index_node.index_name() == "idx_age", "CREATE INDEX index name mismatch");
    require(create_index_node.index_kind() == litedb::core::meta::entry::IndexKind::BTree, "CREATE INDEX kind value mismatch");
    require(create_index_node.if_not_exists(), "CREATE INDEX if not exists mismatch");
    require(!create_index_node.unique(), "CREATE INDEX unique mismatch");

    auto create_vector_index = plan_ok(
        fixture,
        "CREATE VINDEX IF NOT EXISTS vidx_embedding ON users (embedding) USING HNSW "
        "WITH (metric = COSINE, max_neighbors = 24, ef_construction = 240, ef_search = 80, random_seed = 7);"
    );
    require(create_vector_index->kind() == LogicalStatementPlanKind::CreateVectorIndex, "CREATE VINDEX kind mismatch");
    const auto & create_vector_index_node = static_cast<const CreateVectorIndexPlan &>(*create_vector_index);
    require(create_vector_index_node.database_id() == fixture.database_id, "CREATE VINDEX database id mismatch");
    require(create_vector_index_node.collection_id() == fixture.users_id, "CREATE VINDEX collection id mismatch");
    require(create_vector_index_node.collection_name() == "users", "CREATE VINDEX collection name mismatch");
    require(create_vector_index_node.column_name() == "embedding", "CREATE VINDEX column name mismatch");
    require(create_vector_index_node.index_name() == "vidx_embedding", "CREATE VINDEX index name mismatch");
    require(create_vector_index_node.index_kind() == VectorIndexKind::Hnsw, "CREATE VINDEX kind value mismatch");
    require(create_vector_index_node.metric() == VectorDistanceMetric::Cosine, "CREATE VINDEX metric mismatch");
    require(create_vector_index_node.max_neighbors() == 24, "CREATE VINDEX max_neighbors mismatch");
    require(create_vector_index_node.ef_construction() == 240, "CREATE VINDEX ef_construction mismatch");
    require(create_vector_index_node.ef_search_default() == 80, "CREATE VINDEX ef_search mismatch");
    require(create_vector_index_node.random_seed() == 7, "CREATE VINDEX random_seed mismatch");
    require(create_vector_index_node.if_not_exists(), "CREATE VINDEX if not exists mismatch");

    auto drop_database = plan_ok(fixture, "DROP DATABASE IF EXISTS missing;");
    require(drop_database->kind() == LogicalStatementPlanKind::DropDatabase, "DROP DATABASE kind mismatch");
    const auto & drop_database_node = static_cast<const DropDatabasePlan &>(*drop_database);
    require(drop_database_node.if_exists(), "DROP DATABASE if exists mismatch");
    require(drop_database_node.database_name() == "missing", "DROP DATABASE name mismatch");

    auto drop_collection = plan_ok(fixture, "DROP COLLECTION IF EXISTS missing;");
    require(drop_collection->kind() == LogicalStatementPlanKind::DropCollection, "DROP COLLECTION kind mismatch");
    const auto & drop_collection_node = static_cast<const DropCollectionPlan &>(*drop_collection);
    require(drop_collection_node.database_id() == fixture.database_id, "DROP COLLECTION database id mismatch");
    require(drop_collection_node.if_exists(), "DROP COLLECTION if exists mismatch");

    const auto * age_column = fixture.catalog.find_column(fixture.users_id, "age");
    require(age_column != nullptr, "age column lookup failed");
    auto created_index = fixture.catalog.create_index(CreateIndexRequest {
        .collection_id = fixture.users_id,
        .column_ids = {age_column->id()},
        .name = "idx_age",
        .kind = litedb::core::meta::entry::IndexKind::BTree,
    });
    require(created_index.has_value(), "fixture index create failed");

    auto drop_index = plan_ok(fixture, "DROP INDEX idx_age ON users;");
    require(drop_index->kind() == LogicalStatementPlanKind::DropIndex, "DROP INDEX kind mismatch");
    const auto & drop_index_node = static_cast<const DropIndexPlan &>(*drop_index);
    require(drop_index_node.database_id() == fixture.database_id, "DROP INDEX database id mismatch");
    require(drop_index_node.collection_id() == fixture.users_id, "DROP INDEX collection id mismatch");
    require(drop_index_node.collection_name() == "users", "DROP INDEX collection name mismatch");
    require(drop_index_node.index_name() == "idx_age", "DROP INDEX index name mismatch");
    require(!drop_index_node.if_exists(), "DROP INDEX if exists mismatch");

    const auto * embedding_column = fixture.catalog.find_column(fixture.users_id, "embedding");
    require(embedding_column != nullptr, "embedding column lookup failed");
    auto created_vector_index = fixture.catalog.create_vector_index(CreateVectorIndexRequest {
        .collection_id = fixture.users_id,
        .column_id = embedding_column->id(),
        .name = "vidx_embedding",
    });
    require(created_vector_index.has_value(), "fixture vector index create failed");

    auto drop_vector_index = plan_ok(fixture, "DROP VINDEX vidx_embedding ON users;");
    require(drop_vector_index->kind() == LogicalStatementPlanKind::DropVectorIndex, "DROP VINDEX kind mismatch");
    const auto & drop_vector_index_node = static_cast<const DropVectorIndexPlan &>(*drop_vector_index);
    require(drop_vector_index_node.database_id() == fixture.database_id, "DROP VINDEX database id mismatch");
    require(drop_vector_index_node.collection_id() == fixture.users_id, "DROP VINDEX collection id mismatch");
    require(drop_vector_index_node.collection_name() == "users", "DROP VINDEX collection name mismatch");
    require(drop_vector_index_node.index_name() == "vidx_embedding", "DROP VINDEX index name mismatch");
    require(!drop_vector_index_node.if_exists(), "DROP VINDEX if exists mismatch");

    require(plan_ok(fixture, "SHOW DATABASES;")->kind() == LogicalStatementPlanKind::ShowDatabases, "SHOW DATABASES kind mismatch");

    auto show_collections = plan_ok(fixture, "SHOW COLLECTIONS;");
    require(show_collections->kind() == LogicalStatementPlanKind::ShowCollections, "SHOW COLLECTIONS kind mismatch");
    require(static_cast<const ShowCollectionsPlan &>(*show_collections).database_id() == fixture.database_id, "SHOW COLLECTIONS database id mismatch");

    auto show_indexes = plan_ok(fixture, "SHOW INDEXES FROM users;");
    require(show_indexes->kind() == LogicalStatementPlanKind::ShowIndexes, "SHOW INDEXES kind mismatch");
    const auto & show_indexes_node = static_cast<const ShowIndexesPlan &>(*show_indexes);
    require(show_indexes_node.database_id() == fixture.database_id, "SHOW INDEXES database id mismatch");
    require(show_indexes_node.collection_id() == fixture.users_id, "SHOW INDEXES collection id mismatch");
    require(show_indexes_node.collection_name() == "users", "SHOW INDEXES collection name mismatch");

    auto show_vector_indexes = plan_ok(fixture, "SHOW VINDEXES FROM users;");
    require(show_vector_indexes->kind() == LogicalStatementPlanKind::ShowVectorIndexes, "SHOW VINDEXES kind mismatch");
    const auto & show_vector_indexes_node = static_cast<const ShowVectorIndexesPlan &>(*show_vector_indexes);
    require(show_vector_indexes_node.database_id() == fixture.database_id, "SHOW VINDEXES database id mismatch");
    require(show_vector_indexes_node.collection_id() == fixture.users_id, "SHOW VINDEXES collection id mismatch");
    require(show_vector_indexes_node.collection_name() == "users", "SHOW VINDEXES collection name mismatch");

    auto describe = plan_ok(fixture, "DESCRIBE users;");
    require(describe->kind() == LogicalStatementPlanKind::DescribeCollection, "DESCRIBE kind mismatch");
    const auto & describe_node = static_cast<const DescribeCollectionPlan &>(*describe);
    require(describe_node.database_id() == fixture.database_id, "DESCRIBE database id mismatch");
    require(describe_node.collection_id() == fixture.users_id, "DESCRIBE collection id mismatch");
}

void test_null_statement_error()
{
    LogicalPlanner planner;
    auto result = planner.plan(nullptr);
    require(!result.has_value(), "null statement should fail");
    require(result.error().code == PlannerErrorCode::InvalidArgument, "null statement error code mismatch");
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
        test_null_statement_error();
    } catch (const std::exception & exception) {
        std::cerr << exception.what() << '\n';
        return 1;
    }

    return 0;
}

#include "core/binder/binder.hpp"
#include "core/binder/bound/expression/bound_literal_expression.hpp"
#include "core/meta/meta_engine.hpp"
#include "core/executor/executor.hpp"
#include "core/index/index_engine.hpp"
#include "core/index/scalar_index_key.hpp"
#include "core/optimizer/optimizer.hpp"
#include "core/parser/parser.hpp"
#include "core/parser/ast/statement/statement_node.hpp"
#include "core/physical_planner/physical_planner.hpp"
#include "core/physical_planner/statement/physical_insert_plan.hpp"
#include "core/physical_planner/statement/physical_statement_plan.hpp"
#include "core/logical_planner/logical_planner.hpp"
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
#include <vector>

namespace
{

using namespace litedb::core::binder;
using namespace litedb::core::binder::bound;
using namespace litedb::core::meta;
using namespace litedb::core::meta::entry;
using namespace litedb::core::common;
using namespace litedb::core::executor;
using namespace litedb::core::index;
using namespace litedb::core::optimizer;
using namespace litedb::core::parser;
using namespace litedb::core::parser::ast;
using namespace litedb::core::physical_plan;
using namespace litedb::core::planner;
using namespace litedb::core::planner::logical;
using namespace litedb::core::logical_planner;
using namespace litedb::core::schema;
using namespace litedb::core::storage;

constexpr AstNodeLocation loc {1, 1};

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

template <typename T>
const T & get_value(const Value & value)
{
    return std::get<T>(value.data());
}

std::unique_ptr<StatementNode> parse_ok(std::string_view sql)
{
    Parser parser {std::string(sql)};
    auto result = parser.parse();
    if (!result.has_value()) {
        throw std::runtime_error(result.error().message());
    }
    return std::move(*result);
}

std::unique_ptr<PhysicalStatementPlan> plan_ok(
    CatalogEditor & catalog,
    IndexEngine & index_engine,
    std::string_view sql,
    std::optional<DatabaseId> database_id = std::nullopt
)
{
    auto statement = parse_ok(sql);
    SessionContext session {.current_database_id = database_id};
    BinderContext context {catalog.view(), session};
    Binder binder {context};
    auto bound = binder.bind(*statement);
    if (!bound.has_value()) {
        throw std::runtime_error(bound.error().message());
    }

    (void) index_engine;
    LogicalPlanner planner;
    auto planned = planner.plan(std::move(*bound));
    if (!planned.has_value()) {
        throw std::runtime_error(planned.error().message());
    }

    Optimizer optimizer {{}, catalog.view()};
    auto optimized = optimizer.optimize(std::move(*planned));
    if (!optimized.has_value()) {
        throw std::runtime_error(optimized.error().message());
    }

    PhysicalPlanner physical_planner;
    return physical_planner.plan(**optimized);
}

ExecutionResult execute_ok(
    CatalogEditor & catalog,
    StorageEngine & storage,
    IndexEngine & index_engine,
    litedb::core::vindex::VectorIndexEngine & vector_index_engine,
    litedb::core::transaction::TransactionManager & transaction_manager,
    std::string_view sql,
    std::optional<DatabaseId> database_id = std::nullopt
)
{
    auto plan = plan_ok(catalog, index_engine, sql, database_id);
    Executor executor {catalog.view(), storage, index_engine, vector_index_engine, transaction_manager};
    auto result = executor.execute(*plan);
    if (!result.has_value()) {
        throw std::runtime_error(result.error().message());
    }
    return std::move(*result);
}

ExecutionError execute_error(
    CatalogEditor & catalog,
    StorageEngine & storage,
    IndexEngine & index_engine,
    litedb::core::vindex::VectorIndexEngine & vector_index_engine,
    litedb::core::transaction::TransactionManager & transaction_manager,
    std::string_view sql,
    std::optional<DatabaseId> database_id = std::nullopt
)
{
    auto plan = plan_ok(catalog, index_engine, sql, database_id);
    Executor executor {catalog.view(), storage, index_engine, vector_index_engine, transaction_manager};
    auto result = executor.execute(*plan);
    require(!result.has_value(), "statement should fail to execute");
    return std::move(result.error());
}

struct Fixture
{
    litedb::tests::TemporaryDirectory storage_directory {"litedb-executor-tests"};
    litedb::core::filesystem::FileSystem filesystem {litedb::core::filesystem::create_platform_filesystem()};
    CatalogEditor catalog;
    CatalogPublisher publisher {storage_directory.path() / "meta.ldb", filesystem};
    StorageEngine storage {
        storage_directory.path(),
        filesystem,
        litedb::core::storage::StorageOpenMode::TransactionalStaging,
    };
    IndexEngine index_engine {storage_directory.path(), filesystem};
    litedb::core::vindex::VectorIndexEngine vector_index_engine {storage_directory.path() / "vindexes", filesystem};
    std::optional<litedb::core::wal::WalManager> wal_store;
    std::unique_ptr<litedb::core::transaction::TransactionManager> transaction_manager;
    DatabaseId database_id {0};
    CollectionId users_id {0};

    Fixture()
    {
        auto created_database = catalog.create_database(CreateDatabaseRequest {.name = "demo"});
        require(created_database.has_value(), "fixture database creation failed");
        database_id = *created_database;

        auto created_collection = catalog.create_collection(CreateCollectionRequest {
            .database_id = database_id,
            .name = "users",
            .columns = {
                ColumnDefinition {.name = "id", .type = type(LogicalTypeId::BigInt)},
                ColumnDefinition {.name = "name", .type = type(LogicalTypeId::Varchar, 64)},
                ColumnDefinition {.name = "age", .type = type(LogicalTypeId::Integer)},
                ColumnDefinition {.name = "embedding", .type = type(LogicalTypeId::Vector, 3)},
            },
        });
        require(created_collection.has_value(), "fixture collection creation failed");
        users_id = *created_collection;

        const auto * collection = catalog.view().find_collection(database_id, "users");
        require(collection != nullptr, "created collection missing");
        auto collection_schema = load_collection_schema(catalog.view(), users_id);
        require(collection_schema.has_value(), "fixture schema load failed");
        require(storage.create_collection(std::move(*collection_schema)).has_value(), "fixture storage creation failed");
        require(storage.contains_collection(users_id), "created collection storage missing");
        auto opened_wal = litedb::core::wal::WalManager::open(storage_directory.path() / "wal", filesystem);
        require(opened_wal.has_value(), "fixture WAL creation failed");
        wal_store = std::move(*opened_wal);
        require(publisher.open_or_initialize().has_value(), "fixture catalog publisher open failed");
        require(publisher.publish_committed(catalog.snapshot()).has_value(), "fixture catalog publish failed");
        transaction_manager = std::make_unique<litedb::core::transaction::TransactionManager>(
            storage_directory.path(), filesystem, publisher, storage, index_engine, vector_index_engine, *wal_store, 0
        );
    }
};

IndexId create_index(
    Fixture & fixture,
    std::string name,
    litedb::core::meta::entry::IndexKind kind = litedb::core::meta::entry::IndexKind::BTree
)
{
    const auto * column = fixture.catalog.view().find_column(fixture.users_id, "age");
    require(column != nullptr, "age column missing");
    auto created = fixture.catalog.create_index(CreateIndexRequest {
        .collection_id = fixture.users_id,
        .column_ids = {column->id()},
        .name = std::move(name),
        .kind = kind,
    });
    require(created.has_value(), "fixture index creation failed");
    require(fixture.publisher.publish_committed(fixture.catalog.snapshot()).has_value(),
            "fixture index catalog publish failed");
    const auto * entry = fixture.catalog.view().find_index(*created);
    require(entry != nullptr, "fixture index metadata missing");
    auto schema = load_collection_schema(fixture.catalog.view(), fixture.users_id);
    require(schema.has_value(), "fixture index schema load failed");
    require(
        fixture.index_engine.create_index(*entry, *schema, fixture.storage).has_value(),
        "fixture index create failed"
    );
    return *created;
}

void insert_user(Fixture & fixture, std::int64_t id, std::string_view name, std::int32_t age)
{
    auto result = execute_ok(
        fixture.catalog,
        fixture.storage,
        fixture.index_engine,
        fixture.vector_index_engine,
        *fixture.transaction_manager,
        "INSERT INTO users VALUES ("
            + std::to_string(id)
            + ", '"
            + std::string(name)
            + "', "
            + std::to_string(age)
            + ", [0.1, 0.2, 0.3]);",
        fixture.database_id
    );
    require(result.affected_rows == 1, "INSERT affected rows mismatch");
}

std::vector<RecordId> find_index_equal(Fixture & fixture, IndexId index_id, Value value)
{
    auto key = ScalarIndexKey::from_value(std::move(value));
    require(key.has_value(), "index key creation failed");

    auto index_view = fixture.index_engine.find_index(index_id);
    require(index_view.has_value(), "managed index missing");

    auto found = fixture.index_engine.find_equal(index_id, *key);
    require(found.has_value(), "index lookup failed");
    return std::move(*found);
}

void test_use_show_and_describe()
{
    Fixture fixture;

    SessionContext session {.current_database_id = fixture.database_id};
    auto use_result = execute_ok(fixture.catalog, fixture.storage, fixture.index_engine, fixture.vector_index_engine, *fixture.transaction_manager, "USE demo;");
    require(use_result.kind == ExecutionResultKind::UseDatabase, "USE result kind mismatch");
    require(use_result.selected_database_id.value() == fixture.database_id, "USE selected database id mismatch");
    require(use_result.selected_database_name.value() == "demo", "USE selected database name mismatch");
    require(session.current_database_id.value() == fixture.database_id, "USE should not mutate external session");

    auto databases = execute_ok(fixture.catalog, fixture.storage, fixture.index_engine, fixture.vector_index_engine, *fixture.transaction_manager, "SHOW DATABASES;", fixture.database_id);
    require(databases.kind == ExecutionResultKind::RowSet, "SHOW DATABASES result kind mismatch");
    require(databases.columns.size() == 1, "SHOW DATABASES column count mismatch");
    require(databases.rows.size() == 1, "SHOW DATABASES row count mismatch");
    require(get_value<std::string>(databases.rows[0].values[0]) == "demo", "SHOW DATABASES value mismatch");

    auto collections = execute_ok(fixture.catalog, fixture.storage, fixture.index_engine, fixture.vector_index_engine, *fixture.transaction_manager, "SHOW COLLECTIONS;", fixture.database_id);
    require(collections.rows.size() == 1, "SHOW COLLECTIONS row count mismatch");
    require(get_value<std::string>(collections.rows[0].values[0]) == "users", "SHOW COLLECTIONS value mismatch");

    (void) create_index(fixture, "idx_age");

    auto indexes = execute_ok(fixture.catalog, fixture.storage, fixture.index_engine, fixture.vector_index_engine, *fixture.transaction_manager, "SHOW INDEXES FROM users;", fixture.database_id);
    require(indexes.kind == ExecutionResultKind::RowSet, "SHOW INDEXES result kind mismatch");
    require(indexes.columns.size() == 4, "SHOW INDEXES column count mismatch");
    require(indexes.rows.size() == 1, "SHOW INDEXES row count mismatch");
    require(get_value<std::string>(indexes.rows[0].values[0]) == "idx_age", "SHOW INDEXES index name mismatch");
    require(get_value<std::string>(indexes.rows[0].values[1]) == "age", "SHOW INDEXES column name mismatch");
    require(get_value<std::string>(indexes.rows[0].values[2]) == "BTREE", "SHOW INDEXES type mismatch");
    require(!get_value<bool>(indexes.rows[0].values[3]), "SHOW INDEXES unique mismatch");

    const auto * embedding = fixture.catalog.view().find_column(fixture.users_id, "embedding");
    require(embedding != nullptr, "embedding column missing");
    auto created_vector_index = fixture.catalog.create_vector_index(CreateVectorIndexRequest {
        .collection_id = fixture.users_id,
        .column_id = embedding->id(),
        .name = "vidx_embedding",
        .kind = VectorIndexKind::Hnsw,
        .metric = VectorDistanceMetric::Cosine,
        .hnsw_options = {
            .max_neighbors = 24,
            .ef_construction = 240,
            .ef_search_default = 80,
            .random_seed = 7,
        },
    });
    require(created_vector_index.has_value(), "fixture vector index creation failed");
    require(fixture.publisher.publish_committed(fixture.catalog.snapshot()).has_value(),
            "fixture vector index catalog publish failed");

    auto vector_indexes = execute_ok(fixture.catalog, fixture.storage, fixture.index_engine, fixture.vector_index_engine, *fixture.transaction_manager, "SHOW VINDEXES FROM users;", fixture.database_id);
    require(vector_indexes.kind == ExecutionResultKind::RowSet, "SHOW VINDEXES result kind mismatch");
    require(vector_indexes.columns.size() == 9, "SHOW VINDEXES column count mismatch");
    require(vector_indexes.rows.size() == 1, "SHOW VINDEXES row count mismatch");
    require(get_value<std::string>(vector_indexes.rows[0].values[0]) == "vidx_embedding", "SHOW VINDEXES index name mismatch");
    require(get_value<std::string>(vector_indexes.rows[0].values[1]) == "embedding", "SHOW VINDEXES column name mismatch");
    require(get_value<std::string>(vector_indexes.rows[0].values[2]) == "HNSW", "SHOW VINDEXES type mismatch");
    require(get_value<std::string>(vector_indexes.rows[0].values[3]) == "COSINE", "SHOW VINDEXES metric mismatch");
    require(get_value<std::int64_t>(vector_indexes.rows[0].values[4]) == 3, "SHOW VINDEXES dimension mismatch");
    require(get_value<std::int64_t>(vector_indexes.rows[0].values[5]) == 24, "SHOW VINDEXES max_neighbors mismatch");
    require(get_value<std::int64_t>(vector_indexes.rows[0].values[6]) == 240, "SHOW VINDEXES ef_construction mismatch");
    require(get_value<std::int64_t>(vector_indexes.rows[0].values[7]) == 80, "SHOW VINDEXES ef_search mismatch");
    require(get_value<std::int64_t>(vector_indexes.rows[0].values[8]) == 7, "SHOW VINDEXES random_seed mismatch");

    auto describe = execute_ok(fixture.catalog, fixture.storage, fixture.index_engine, fixture.vector_index_engine, *fixture.transaction_manager, "DESCRIBE users;", fixture.database_id);
    require(describe.columns.size() == 6, "DESCRIBE column count mismatch");
    require(describe.rows.size() == 4, "DESCRIBE row count mismatch");
    require(get_value<std::string>(describe.rows[0].values[0]) == "id", "DESCRIBE column name mismatch");
    require(get_value<std::string>(describe.rows[0].values[1]) == "BIGINT", "DESCRIBE type mismatch");
    require(get_value<bool>(describe.rows[0].values[2]), "DESCRIBE nullable mismatch");
}

void test_insert_select_update_and_delete()
{
    Fixture fixture;
    insert_user(fixture, 1, "alice", 18);
    insert_user(fixture, 2, "bob", 20);
    insert_user(fixture, 3, "carl", 15);

    auto selected = execute_ok(
        fixture.catalog,
        fixture.storage,
        fixture.index_engine,
        fixture.vector_index_engine,
        *fixture.transaction_manager,
        "SELECT name FROM users WHERE age >= 18 ORDER BY age DESC LIMIT 2 OFFSET 0;",
        fixture.database_id
    );
    require(selected.kind == ExecutionResultKind::RowSet, "SELECT result kind mismatch");
    require(selected.columns.size() == 1, "SELECT column count mismatch");
    require(selected.columns[0].name == "name", "SELECT projection column name mismatch");
    require(selected.rows.size() == 2, "SELECT row count mismatch");
    require(get_value<std::string>(selected.rows[0].values[0]) == "bob", "SELECT order mismatch");
    require(get_value<std::string>(selected.rows[1].values[0]) == "alice", "SELECT order mismatch");

    auto alias_order = execute_ok(
        fixture.catalog,
        fixture.storage,
        fixture.index_engine,
        fixture.vector_index_engine,
        *fixture.transaction_manager,
        "SELECT age + 1 AS next_age FROM users ORDER BY next_age ASC;",
        fixture.database_id
    );
    require(alias_order.columns.size() == 1, "SELECT alias column count mismatch");
    require(alias_order.columns[0].name == "next_age", "SELECT alias column name mismatch");
    require(alias_order.rows.size() == 3, "SELECT alias row count mismatch");
    require(get_value<std::int32_t>(alias_order.rows[0].values[0]) == 16, "ORDER BY alias first value mismatch");
    require(get_value<std::int32_t>(alias_order.rows[1].values[0]) == 19, "ORDER BY alias second value mismatch");
    require(get_value<std::int32_t>(alias_order.rows[2].values[0]) == 21, "ORDER BY alias third value mismatch");

    auto column_alias = execute_ok(
        fixture.catalog,
        fixture.storage,
        fixture.index_engine,
        fixture.vector_index_engine,
        *fixture.transaction_manager,
        "SELECT id AS user_id FROM users ORDER BY user_id ASC;",
        fixture.database_id
    );
    require(column_alias.columns[0].name == "user_id", "SELECT column alias name mismatch");

    auto expression_without_alias = execute_ok(
        fixture.catalog,
        fixture.storage,
        fixture.index_engine,
        fixture.vector_index_engine,
        *fixture.transaction_manager,
        "SELECT age + 1 FROM users ORDER BY age ASC;",
        fixture.database_id
    );
    require(expression_without_alias.columns[0].name == "expr1", "SELECT expression fallback name mismatch");

    auto update = execute_ok(fixture.catalog, fixture.storage, fixture.index_engine, fixture.vector_index_engine, *fixture.transaction_manager, "UPDATE users SET age = age + 1 WHERE id = 1;", fixture.database_id);
    require(update.kind == ExecutionResultKind::Command, "UPDATE result kind mismatch");
    require(update.affected_rows == 1, "UPDATE affected rows mismatch");

    auto updated = execute_ok(fixture.catalog, fixture.storage, fixture.index_engine, fixture.vector_index_engine, *fixture.transaction_manager, "SELECT age FROM users WHERE id = 1;", fixture.database_id);
    require(updated.rows.size() == 1, "updated SELECT row count mismatch");
    require(get_value<std::int32_t>(updated.rows[0].values[0]) == 19, "updated value mismatch");

    auto before_delete_cursor = fixture.storage.scan(fixture.users_id);
    require(before_delete_cursor.has_value(), "scan failed before delete");
    auto first_record = before_delete_cursor->next();
    require(first_record && first_record->has_value(), "first record missing before delete");
    const auto first_record_id = (**first_record).record_id;

    auto del = execute_ok(fixture.catalog, fixture.storage, fixture.index_engine, fixture.vector_index_engine, *fixture.transaction_manager, "DELETE FROM users WHERE age < 18;", fixture.database_id);
    require(del.affected_rows == 1, "DELETE affected rows mismatch");

    auto remaining = execute_ok(fixture.catalog, fixture.storage, fixture.index_engine, fixture.vector_index_engine, *fixture.transaction_manager, "SELECT id FROM users ORDER BY id ASC;", fixture.database_id);
    require(remaining.rows.size() == 2, "remaining row count mismatch");
    require(get_value<std::int64_t>(remaining.rows[0].values[0]) == 1, "remaining first id mismatch");
    require(get_value<std::int64_t>(remaining.rows[1].values[0]) == 2, "remaining second id mismatch");

    auto after_update_cursor = fixture.storage.scan(fixture.users_id);
    require(after_update_cursor.has_value(), "scan failed after update/delete");
    auto first_after_update = after_update_cursor->next();
    require(first_after_update && first_after_update->has_value(), "first record missing after update/delete");
    require((**first_after_update).record_id == first_record_id, "UPDATE should preserve record id");
}

void test_order_by_keeps_null_last()
{
    Fixture fixture;
    insert_user(fixture, 1, "alice", 18);
    insert_user(fixture, 2, "bob", 20);

    auto null_age = execute_ok(
        fixture.catalog,
        fixture.storage,
        fixture.index_engine,
        fixture.vector_index_engine,
        *fixture.transaction_manager,
        "INSERT INTO users (id, name, embedding) VALUES (3, 'null-age', [0.1, 0.2, 0.3]);",
        fixture.database_id
    );
    require(null_age.affected_rows == 1, "null age INSERT affected rows mismatch");

    auto selected = execute_ok(
        fixture.catalog,
        fixture.storage,
        fixture.index_engine,
        fixture.vector_index_engine,
        *fixture.transaction_manager,
        "SELECT name FROM users ORDER BY age DESC;",
        fixture.database_id
    );

    require(selected.rows.size() == 3, "NULL order row count mismatch");
    require(get_value<std::string>(selected.rows[0].values[0]) == "bob", "DESC order first mismatch");
    require(get_value<std::string>(selected.rows[1].values[0]) == "alice", "DESC order second mismatch");
    require(get_value<std::string>(selected.rows[2].values[0]) == "null-age", "NULL should sort last");
}

void test_index_scan_execution_paths()
{
    Fixture fixture;
    (void) create_index(fixture, "idx_age_explicit");

    insert_user(fixture, 1, "alice", 18);
    insert_user(fixture, 2, "bob", 20);
    insert_user(fixture, 3, "carl", 15);
    insert_user(fixture, 4, "dora", 18);

    auto equal = execute_ok(
        fixture.catalog,
        fixture.storage,
        fixture.index_engine,
        fixture.vector_index_engine,
        *fixture.transaction_manager,
        "SELECT id FROM users WHERE age = 18 ORDER BY id ASC;",
        fixture.database_id
    );
    require(equal.rows.size() == 2, "btree equality SELECT row count mismatch");
    require(get_value<std::int64_t>(equal.rows[0].values[0]) == 1, "btree equality first row mismatch");
    require(get_value<std::int64_t>(equal.rows[1].values[0]) == 4, "btree equality second row mismatch");

    auto btree_range_filter = execute_ok(
        fixture.catalog,
        fixture.storage,
        fixture.index_engine,
        fixture.vector_index_engine,
        *fixture.transaction_manager,
        "SELECT id FROM users WHERE age >= 18 ORDER BY id ASC;",
        fixture.database_id
    );
    require(btree_range_filter.rows.size() == 3, "btree range SELECT row count mismatch");

    const auto btree_index_id = create_index(fixture, "idx_age_btree");

    auto btree_range = execute_ok(
        fixture.catalog,
        fixture.storage,
        fixture.index_engine,
        fixture.vector_index_engine,
        *fixture.transaction_manager,
        "SELECT id FROM users WHERE age BETWEEN 18 AND 20 ORDER BY id ASC;",
        fixture.database_id
    );
    require(btree_range.rows.size() == 3, "btree range SELECT row count mismatch");

    auto updated = execute_ok(
        fixture.catalog,
        fixture.storage,
        fixture.index_engine,
        fixture.vector_index_engine,
        *fixture.transaction_manager,
        "UPDATE users SET name = 'adult' WHERE age >= 20;",
        fixture.database_id
    );
    require(updated.affected_rows == 1, "indexed range UPDATE affected rows mismatch");

    auto renamed = execute_ok(
        fixture.catalog,
        fixture.storage,
        fixture.index_engine,
        fixture.vector_index_engine,
        *fixture.transaction_manager,
        "SELECT name FROM users WHERE id = 2;",
        fixture.database_id
    );
    require(get_value<std::string>(renamed.rows[0].values[0]) == "adult", "indexed UPDATE value mismatch");

    auto deleted = execute_ok(
        fixture.catalog,
        fixture.storage,
        fixture.index_engine,
        fixture.vector_index_engine,
        *fixture.transaction_manager,
        "DELETE FROM users WHERE age < 18;",
        fixture.database_id
    );
    require(deleted.affected_rows == 1, "indexed range DELETE affected rows mismatch");

    require(fixture.catalog.drop_index(DropIndexRequest {
        .collection_id = fixture.users_id,
        .name = "idx_age_btree",
    }).has_value(), "fixture index metadata drop failed");
    require(fixture.publisher.publish_committed(fixture.catalog.snapshot()).has_value(),
            "fixture index drop catalog publish failed");
    require(fixture.index_engine.drop_index(btree_index_id).has_value(), "fixture runtime index drop failed");

    auto range_after_drop = execute_ok(
        fixture.catalog,
        fixture.storage,
        fixture.index_engine,
        fixture.vector_index_engine,
        *fixture.transaction_manager,
        "SELECT id FROM users WHERE age >= 18 ORDER BY id ASC;",
        fixture.database_id
    );
    require(range_after_drop.rows.size() == 3, "range after btree drop fallback row count mismatch");
}

void test_error_mapping()
{
    Fixture fixture;
    auto dropped_storage = fixture.storage.drop_collection(fixture.users_id);
    require(dropped_storage.has_value(), "fixture storage drop failed");

    auto missing_storage = execute_error(fixture.catalog, fixture.storage, fixture.index_engine, fixture.vector_index_engine, *fixture.transaction_manager, "SELECT * FROM users;", fixture.database_id);
    require(missing_storage.is(ExecutionErrorCode::CollectionNotFound), "missing storage error mismatch");

    auto schema = load_collection_schema(fixture.catalog.view(), fixture.users_id);
    require(schema.has_value(), "schema reload failed");
    auto recreated = fixture.storage.create_collection(std::move(*schema));
    require(recreated.has_value(), "storage recreate failed");

    std::vector<BoundColumn> columns;
    std::vector<std::unique_ptr<BoundExpression>> values;
    values.push_back(std::make_unique<BoundLiteralExpression>(type(LogicalTypeId::Integer), "not-an-int", loc));
    PhysicalInsertPlan bad_insert {
        fixture.database_id,
        fixture.users_id,
        "users",
        std::move(columns),
        std::move(values),
        loc,
    };

    Executor executor {
        fixture.catalog.view(), fixture.storage, fixture.index_engine, fixture.vector_index_engine, *fixture.transaction_manager
    };
    auto invalid_literal = executor.execute(bad_insert);
    require(!invalid_literal.has_value(), "invalid literal INSERT should fail");
    require(invalid_literal.error().is(ExecutionErrorCode::EvaluationError), "evaluation error mapping mismatch");
}

void test_ddl_requires_database_engine()
{
    Fixture fixture;
    auto plan = plan_ok(
        fixture.catalog,
        fixture.index_engine,
        "CREATE INDEX idx_age ON users (age);",
        fixture.database_id
    );
    Executor executor {
        fixture.catalog.view(), fixture.storage, fixture.index_engine, fixture.vector_index_engine, *fixture.transaction_manager
    };
    auto result = executor.execute(*plan);
    require(!result.has_value(), "Executor should reject standalone DDL");
    require(result.error().is(ExecutionErrorCode::UnsupportedStatement), "standalone DDL error mismatch");
}

} // namespace

int main()
{
    try {
        test_use_show_and_describe();
        test_insert_select_update_and_delete();
        test_order_by_keeps_null_last();
        test_index_scan_execution_paths();
        test_error_mapping();
        test_ddl_requires_database_engine();
    } catch (const std::exception & exception) {
        std::cerr << exception.what() << '\n';
        return 1;
    }

    return 0;
}

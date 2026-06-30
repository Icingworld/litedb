#include "core/binder/binder.hpp"
#include "core/binder/bound/expression/bound_literal_expression.hpp"
#include "core/catalog/in_memory_catalog.hpp"
#include "core/executor/executor.hpp"
#include "core/index/index_manager.hpp"
#include "core/index/scalar_index_key.hpp"
#include "core/parser/parser.hpp"
#include "core/parser/ast/statement/statement_node.hpp"
#include "core/planner/planner.hpp"
#include "core/planner/statement/insert_plan.hpp"
#include "core/schema/schema_loader.hpp"
#include "core/storage/storage_manager.hpp"

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
using namespace litedb::core::catalog;
using namespace litedb::core::common;
using namespace litedb::core::executor;
using namespace litedb::core::index;
using namespace litedb::core::parser;
using namespace litedb::core::parser::ast;
using namespace litedb::core::planner;
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
        throw std::runtime_error(result.error().message);
    }
    return std::move(result.value());
}

std::unique_ptr<StatementPlan> plan_ok(
    InMemoryCatalog & catalog,
    IndexManager & index_manager,
    std::string_view sql,
    std::optional<DatabaseId> database_id = std::nullopt
)
{
    auto statement = parse_ok(sql);
    SessionContext session {.current_database_id = database_id};
    BinderContext context {catalog, session};
    Binder binder {context};
    auto bound = binder.bind(*statement);
    if (!bound.has_value()) {
        throw std::runtime_error(bound.error().message);
    }

    Planner planner {&index_manager};
    auto planned = planner.plan(std::move(bound.value()));
    if (!planned.has_value()) {
        throw std::runtime_error(planned.error().message);
    }
    return std::move(planned.value());
}

ExecutionResult execute_ok(
    InMemoryCatalog & catalog,
    StorageManager & storage,
    IndexManager & index_manager,
    std::string_view sql,
    std::optional<DatabaseId> database_id = std::nullopt
)
{
    auto plan = plan_ok(catalog, index_manager, sql, database_id);
    Executor executor {catalog, storage, index_manager};
    auto result = executor.execute(*plan);
    if (!result.has_value()) {
        throw std::runtime_error(result.error().message);
    }
    return std::move(result.value());
}

ExecutionError execute_error(
    InMemoryCatalog & catalog,
    StorageManager & storage,
    IndexManager & index_manager,
    std::string_view sql,
    std::optional<DatabaseId> database_id = std::nullopt
)
{
    auto plan = plan_ok(catalog, index_manager, sql, database_id);
    Executor executor {catalog, storage, index_manager};
    auto result = executor.execute(*plan);
    require(!result.has_value(), "statement should fail to execute");
    return std::move(result.error());
}

struct Fixture
{
    InMemoryCatalog catalog;
    StorageManager storage;
    IndexManager index_manager;
    DatabaseId database_id {0};
    CollectionId users_id {0};

    Fixture()
    {
        auto create_database = execute_ok(catalog, storage, index_manager, "CREATE DATABASE demo;");
        require(create_database.kind == ExecutionResultKind::Command, "CREATE DATABASE result kind mismatch");
        require(create_database.affected_rows == 1, "CREATE DATABASE affected rows mismatch");

        const auto * database = catalog.find_database("demo");
        require(database != nullptr, "created database missing");
        database_id = database->id();

        auto create_collection = execute_ok(
            catalog,
            storage,
            index_manager,
            "CREATE COLLECTION users ("
            "id BIGINT, "
            "name VARCHAR(64) DEFAULT 'unknown', "
            "age INTEGER, "
            "embedding VECTOR(3)"
            ");",
            database_id
        );
        require(create_collection.affected_rows == 1, "CREATE COLLECTION affected rows mismatch");

        const auto * collection = catalog.find_collection(database_id, "users");
        require(collection != nullptr, "created collection missing");
        users_id = collection->id();
        require(storage.find_collection(users_id) != nullptr, "created collection storage missing");
    }
};

void insert_user(Fixture & fixture, std::int64_t id, std::string_view name, std::int32_t age)
{
    auto result = execute_ok(
        fixture.catalog,
        fixture.storage,
        fixture.index_manager,
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

    auto index_view = fixture.index_manager.find_index(index_id);
    require(index_view.has_value(), "managed index missing");

    auto found = index_view->index.find_equal(key.value());
    require(found.has_value(), "index lookup failed");
    return std::move(found.value());
}

void test_ddl_use_show_and_describe()
{
    Fixture fixture;

    SessionContext session {.current_database_id = fixture.database_id};
    auto use_result = execute_ok(fixture.catalog, fixture.storage, fixture.index_manager, "USE demo;");
    require(use_result.kind == ExecutionResultKind::UseDatabase, "USE result kind mismatch");
    require(use_result.selected_database_id.value() == fixture.database_id, "USE selected database id mismatch");
    require(use_result.selected_database_name.value() == "demo", "USE selected database name mismatch");
    require(session.current_database_id.value() == fixture.database_id, "USE should not mutate external session");

    auto databases = execute_ok(fixture.catalog, fixture.storage, fixture.index_manager, "SHOW DATABASES;", fixture.database_id);
    require(databases.kind == ExecutionResultKind::RowSet, "SHOW DATABASES result kind mismatch");
    require(databases.columns.size() == 1, "SHOW DATABASES column count mismatch");
    require(databases.rows.size() == 1, "SHOW DATABASES row count mismatch");
    require(get_value<std::string>(databases.rows[0].values[0]) == "demo", "SHOW DATABASES value mismatch");

    auto collections = execute_ok(fixture.catalog, fixture.storage, fixture.index_manager, "SHOW COLLECTIONS;", fixture.database_id);
    require(collections.rows.size() == 1, "SHOW COLLECTIONS row count mismatch");
    require(get_value<std::string>(collections.rows[0].values[0]) == "users", "SHOW COLLECTIONS value mismatch");

    auto describe = execute_ok(fixture.catalog, fixture.storage, fixture.index_manager, "DESCRIBE users;", fixture.database_id);
    require(describe.columns.size() == 7, "DESCRIBE column count mismatch");
    require(describe.rows.size() == 4, "DESCRIBE row count mismatch");
    require(get_value<std::string>(describe.rows[0].values[0]) == "id", "DESCRIBE column name mismatch");
    require(get_value<std::string>(describe.rows[0].values[1]) == "BIGINT", "DESCRIBE type mismatch");
    require(!get_value<bool>(describe.rows[0].values[3]), "DESCRIBE primary key mismatch");
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
        fixture.index_manager,
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
        fixture.index_manager,
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
        fixture.index_manager,
        "SELECT id AS user_id FROM users ORDER BY user_id ASC;",
        fixture.database_id
    );
    require(column_alias.columns[0].name == "user_id", "SELECT column alias name mismatch");

    auto expression_without_alias = execute_ok(
        fixture.catalog,
        fixture.storage,
        fixture.index_manager,
        "SELECT age + 1 FROM users ORDER BY age ASC;",
        fixture.database_id
    );
    require(expression_without_alias.columns[0].name == "expr1", "SELECT expression fallback name mismatch");

    auto update = execute_ok(fixture.catalog, fixture.storage, fixture.index_manager, "UPDATE users SET age = age + 1 WHERE id = 1;", fixture.database_id);
    require(update.kind == ExecutionResultKind::Command, "UPDATE result kind mismatch");
    require(update.affected_rows == 1, "UPDATE affected rows mismatch");

    auto updated = execute_ok(fixture.catalog, fixture.storage, fixture.index_manager, "SELECT age FROM users WHERE id = 1;", fixture.database_id);
    require(updated.rows.size() == 1, "updated SELECT row count mismatch");
    require(get_value<std::int32_t>(updated.rows[0].values[0]) == 19, "updated value mismatch");

    auto before_delete_cursor = fixture.storage.find_collection(fixture.users_id)->scan();
    auto first_record = before_delete_cursor->next();
    require(first_record.has_value(), "first record missing before delete");
    const auto first_record_id = first_record->record_id;

    auto del = execute_ok(fixture.catalog, fixture.storage, fixture.index_manager, "DELETE FROM users WHERE age < 18;", fixture.database_id);
    require(del.affected_rows == 1, "DELETE affected rows mismatch");

    auto remaining = execute_ok(fixture.catalog, fixture.storage, fixture.index_manager, "SELECT id FROM users ORDER BY id ASC;", fixture.database_id);
    require(remaining.rows.size() == 2, "remaining row count mismatch");
    require(get_value<std::int64_t>(remaining.rows[0].values[0]) == 1, "remaining first id mismatch");
    require(get_value<std::int64_t>(remaining.rows[1].values[0]) == 2, "remaining second id mismatch");

    auto after_update_cursor = fixture.storage.find_collection(fixture.users_id)->scan();
    auto first_after_update = after_update_cursor->next();
    require(first_after_update.has_value(), "first record missing after update/delete");
    require(first_after_update->record_id == first_record_id, "UPDATE should preserve record id");
}

void test_order_by_keeps_null_last()
{
    Fixture fixture;
    insert_user(fixture, 1, "alice", 18);
    insert_user(fixture, 2, "bob", 20);

    auto null_age = execute_ok(
        fixture.catalog,
        fixture.storage,
        fixture.index_manager,
        "INSERT INTO users (id, name, embedding) VALUES (3, 'null-age', [0.1, 0.2, 0.3]);",
        fixture.database_id
    );
    require(null_age.affected_rows == 1, "null age INSERT affected rows mismatch");

    auto selected = execute_ok(
        fixture.catalog,
        fixture.storage,
        fixture.index_manager,
        "SELECT name FROM users ORDER BY age DESC;",
        fixture.database_id
    );

    require(selected.rows.size() == 3, "NULL order row count mismatch");
    require(get_value<std::string>(selected.rows[0].values[0]) == "bob", "DESC order first mismatch");
    require(get_value<std::string>(selected.rows[1].values[0]) == "alice", "DESC order second mismatch");
    require(get_value<std::string>(selected.rows[2].values[0]) == "null-age", "NULL should sort last");
}

void test_drop_collection_removes_storage()
{
    Fixture fixture;

    auto dropped = execute_ok(fixture.catalog, fixture.storage, fixture.index_manager, "DROP COLLECTION users;", fixture.database_id);
    require(dropped.affected_rows == 1, "DROP COLLECTION affected rows mismatch");
    require(fixture.catalog.find_collection(fixture.users_id) == nullptr, "dropped collection should leave catalog");
    require(fixture.storage.find_collection(fixture.users_id) == nullptr, "dropped collection should leave storage");
}

void test_index_ddl_updates_catalog()
{
    Fixture fixture;

    auto created = execute_ok(fixture.catalog, fixture.storage, fixture.index_manager, "CREATE INDEX idx_age ON users (age);", fixture.database_id);
    require(created.kind == ExecutionResultKind::Command, "CREATE INDEX result kind mismatch");
    require(created.affected_rows == 1, "CREATE INDEX affected rows mismatch");

    const auto * index = fixture.catalog.find_index(fixture.users_id, "idx_age");
    require(index != nullptr, "created index missing");
    const auto index_id = index->id();
    require(index->index_kind() == CatalogIndexKind::BTree, "created index kind mismatch");
    const auto * age_column = fixture.catalog.find_column(fixture.users_id, "age");
    require(age_column != nullptr, "age column lookup failed");
    require(index->column_id() == age_column->id(), "created index column mismatch");
    auto managed_index = fixture.index_manager.find_index(index_id);
    require(managed_index.has_value(), "managed index should be created");
    require(managed_index->index.size() == 0, "empty managed index size mismatch");

    insert_user(fixture, 1, "alice", 18);
    insert_user(fixture, 2, "bob", 20);
    require(find_index_equal(fixture, index_id, Value {std::int32_t {18}}).size() == 1, "INSERT should add index key");
    require(find_index_equal(fixture, index_id, Value {std::int32_t {20}}).size() == 1, "INSERT should add second index key");

    auto updated = execute_ok(
        fixture.catalog,
        fixture.storage,
        fixture.index_manager,
        "UPDATE users SET age = 21 WHERE id = 1;",
        fixture.database_id
    );
    require(updated.affected_rows == 1, "indexed UPDATE affected rows mismatch");
    require(find_index_equal(fixture, index_id, Value {std::int32_t {18}}).empty(), "UPDATE should remove old index key");
    require(find_index_equal(fixture, index_id, Value {std::int32_t {21}}).size() == 1, "UPDATE should add new index key");

    auto deleted = execute_ok(
        fixture.catalog,
        fixture.storage,
        fixture.index_manager,
        "DELETE FROM users WHERE id = 2;",
        fixture.database_id
    );
    require(deleted.affected_rows == 1, "indexed DELETE affected rows mismatch");
    require(find_index_equal(fixture, index_id, Value {std::int32_t {20}}).empty(), "DELETE should remove index key");

    auto duplicate_if_not_exists = execute_ok(
        fixture.catalog,
        fixture.storage,
        fixture.index_manager,
        "CREATE INDEX IF NOT EXISTS idx_age ON users (age) USING BTREE;",
        fixture.database_id
    );
    require(duplicate_if_not_exists.affected_rows == 0, "CREATE INDEX IF NOT EXISTS affected rows mismatch");
    require(fixture.catalog.find_index(fixture.users_id, "idx_age")->index_kind() == CatalogIndexKind::BTree, "existing index should not be replaced");

    auto dropped = execute_ok(fixture.catalog, fixture.storage, fixture.index_manager, "DROP INDEX idx_age ON users;", fixture.database_id);
    require(dropped.affected_rows == 1, "DROP INDEX affected rows mismatch");
    require(fixture.catalog.find_index(fixture.users_id, "idx_age") == nullptr, "dropped index should leave catalog");
    require(!fixture.index_manager.find_index(index_id).has_value(), "dropped index should leave manager");

    auto drop_missing = execute_ok(fixture.catalog, fixture.storage, fixture.index_manager, "DROP INDEX IF EXISTS idx_age ON users;", fixture.database_id);
    require(drop_missing.affected_rows == 0, "DROP INDEX IF EXISTS affected rows mismatch");
}

void test_index_scan_execution_paths()
{
    Fixture fixture;
    auto explicit_btree_index = execute_ok(
        fixture.catalog,
        fixture.storage,
        fixture.index_manager,
        "CREATE INDEX idx_age_explicit ON users (age) USING BTREE;",
        fixture.database_id
    );
    require(explicit_btree_index.affected_rows == 1, "explicit btree index create affected rows mismatch");

    insert_user(fixture, 1, "alice", 18);
    insert_user(fixture, 2, "bob", 20);
    insert_user(fixture, 3, "carl", 15);
    insert_user(fixture, 4, "dora", 18);

    auto equal = execute_ok(
        fixture.catalog,
        fixture.storage,
        fixture.index_manager,
        "SELECT id FROM users WHERE age = 18 ORDER BY id ASC;",
        fixture.database_id
    );
    require(equal.rows.size() == 2, "btree equality SELECT row count mismatch");
    require(get_value<std::int64_t>(equal.rows[0].values[0]) == 1, "btree equality first row mismatch");
    require(get_value<std::int64_t>(equal.rows[1].values[0]) == 4, "btree equality second row mismatch");

    auto btree_range_filter = execute_ok(
        fixture.catalog,
        fixture.storage,
        fixture.index_manager,
        "SELECT id FROM users WHERE age >= 18 ORDER BY id ASC;",
        fixture.database_id
    );
    require(btree_range_filter.rows.size() == 3, "btree range SELECT row count mismatch");

    auto btree_index = execute_ok(
        fixture.catalog,
        fixture.storage,
        fixture.index_manager,
        "CREATE INDEX idx_age_btree ON users (age) USING BTREE;",
        fixture.database_id
    );
    require(btree_index.affected_rows == 1, "btree index create affected rows mismatch");

    auto btree_range = execute_ok(
        fixture.catalog,
        fixture.storage,
        fixture.index_manager,
        "SELECT id FROM users WHERE age BETWEEN 18 AND 20 ORDER BY id ASC;",
        fixture.database_id
    );
    require(btree_range.rows.size() == 3, "btree range SELECT row count mismatch");

    auto updated = execute_ok(
        fixture.catalog,
        fixture.storage,
        fixture.index_manager,
        "UPDATE users SET name = 'adult' WHERE age >= 20;",
        fixture.database_id
    );
    require(updated.affected_rows == 1, "indexed range UPDATE affected rows mismatch");

    auto renamed = execute_ok(
        fixture.catalog,
        fixture.storage,
        fixture.index_manager,
        "SELECT name FROM users WHERE id = 2;",
        fixture.database_id
    );
    require(get_value<std::string>(renamed.rows[0].values[0]) == "adult", "indexed UPDATE value mismatch");

    auto deleted = execute_ok(
        fixture.catalog,
        fixture.storage,
        fixture.index_manager,
        "DELETE FROM users WHERE age < 18;",
        fixture.database_id
    );
    require(deleted.affected_rows == 1, "indexed range DELETE affected rows mismatch");

    auto dropped = execute_ok(
        fixture.catalog,
        fixture.storage,
        fixture.index_manager,
        "DROP INDEX idx_age_btree ON users;",
        fixture.database_id
    );
    require(dropped.affected_rows == 1, "btree index drop affected rows mismatch");

    auto range_after_drop = execute_ok(
        fixture.catalog,
        fixture.storage,
        fixture.index_manager,
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

    auto missing_storage = execute_error(fixture.catalog, fixture.storage, fixture.index_manager, "SELECT * FROM users;", fixture.database_id);
    require(missing_storage.code == ExecutionErrorCode::CollectionStorageNotFound, "missing storage error mismatch");

    auto schema = load_collection_schema(fixture.catalog, fixture.users_id);
    require(schema.has_value(), "schema reload failed");
    auto recreated = fixture.storage.create_collection(std::move(schema.value()));
    require(recreated.has_value(), "storage recreate failed");

    std::vector<BoundColumn> columns;
    std::vector<std::unique_ptr<BoundExpression>> values;
    values.push_back(std::make_unique<BoundLiteralExpression>(type(LogicalTypeId::Integer), "not-an-int", loc));
    InsertPlan bad_insert {
        fixture.database_id,
        fixture.users_id,
        "users",
        std::move(columns),
        std::move(values),
        loc,
    };

    Executor executor {fixture.catalog, fixture.storage, fixture.index_manager};
    auto invalid_literal = executor.execute(bad_insert);
    require(!invalid_literal.has_value(), "invalid literal INSERT should fail");
    require(invalid_literal.error().code == ExecutionErrorCode::EvaluationError, "evaluation error mapping mismatch");
}

} // namespace

int main()
{
    try {
        test_ddl_use_show_and_describe();
        test_insert_select_update_and_delete();
        test_order_by_keeps_null_last();
        test_drop_collection_removes_storage();
        test_index_ddl_updates_catalog();
        test_index_scan_execution_paths();
        test_error_mapping();
    } catch (const std::exception & exception) {
        std::cerr << exception.what() << '\n';
        return 1;
    }

    return 0;
}

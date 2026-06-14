#include "core/engine/engine.hpp"
#include "core/engine/session.hpp"
#include "core/index/scalar_index_key.hpp"

#include <cstdint>
#include <exception>
#include <iostream>
#include <stdexcept>
#include <string>
#include <utility>
#include <vector>

namespace
{

using namespace litedb::core::engine;
using namespace litedb::core::executor;
using namespace litedb::core::index;
using namespace litedb::core::schema;

void require(bool condition, const char * message)
{
    if (!condition) {
        throw std::runtime_error(message);
    }
}

template <typename T>
const T & get_value(const Value & value)
{
    return std::get<T>(value.data());
}

std::vector<litedb::core::common::RecordId> find_index_equal(Engine & engine, litedb::core::common::IndexId index_id, Value value)
{
    auto key = ScalarIndexKey::from_value(std::move(value));
    require(key.has_value(), "index key creation failed");

    auto index_view = engine.index_manager().find_index(index_id);
    require(index_view.has_value(), "managed index missing");

    auto found = index_view->index.find_equal(key.value());
    require(found.has_value(), "index lookup failed");
    return std::move(found.value());
}

ExecutionResult execute_ok(Engine & engine, std::string_view sql)
{
    auto result = engine.execute_sql(sql);
    if (!result.has_value()) {
        throw std::runtime_error(result.error().message);
    }
    return std::move(result.value());
}

EngineError execute_error(Engine & engine, std::string_view sql)
{
    auto result = engine.execute_sql(sql);
    require(!result.has_value(), "SQL should fail");
    return std::move(result.error());
}

void test_execute_sql_end_to_end()
{
    Engine engine;

    auto create_database = execute_ok(engine, "CREATE DATABASE demo;");
    require(create_database.kind == ExecutionResultKind::Command, "CREATE DATABASE result kind mismatch");
    require(create_database.affected_rows == 1, "CREATE DATABASE affected rows mismatch");

    auto use_database = execute_ok(engine, "USE demo;");
    require(use_database.kind == ExecutionResultKind::UseDatabase, "USE result kind mismatch");
    require(engine.current_database_id().has_value(), "Engine session should select database");
    require(use_database.selected_database_id == engine.current_database_id(), "USE selected database mismatch");

    auto create_collection = execute_ok(
        engine,
        "CREATE COLLECTION users ("
        "id BIGINT PRIMARY KEY, "
        "name VARCHAR(64), "
        "age INTEGER"
        ");"
    );
    require(create_collection.affected_rows == 1, "CREATE COLLECTION affected rows mismatch");

    auto insert = execute_ok(engine, "INSERT INTO users VALUES (1, 'alice', 18);");
    require(insert.affected_rows == 1, "INSERT affected rows mismatch");

    auto create_index = execute_ok(engine, "CREATE INDEX idx_age ON users (age);");
    require(create_index.affected_rows == 1, "CREATE INDEX affected rows mismatch");
    const auto * collection = engine.catalog().find_collection(engine.current_database_id().value(), "users");
    require(collection != nullptr, "created collection lookup failed");
    const auto * index = engine.catalog().find_index(collection->id(), "idx_age");
    require(index != nullptr, "created index missing");
    const auto index_id = index->id();

    auto selected = execute_ok(engine, "SELECT name, age FROM users WHERE id = 1;");
    require(selected.kind == ExecutionResultKind::RowSet, "SELECT result kind mismatch");
    require(selected.columns.size() == 2, "SELECT column count mismatch");
    require(selected.rows.size() == 1, "SELECT row count mismatch");
    require(get_value<std::string>(selected.rows[0].values[0]) == "alice", "SELECT name mismatch");
    require(get_value<std::int32_t>(selected.rows[0].values[1]) == 18, "SELECT age mismatch");
    require(find_index_equal(engine, index_id, Value {std::int32_t {18}}).size() == 1, "CREATE INDEX should build existing data");

    auto update = execute_ok(engine, "UPDATE users SET age = 19 WHERE id = 1;");
    require(update.affected_rows == 1, "indexed UPDATE affected rows mismatch");
    require(find_index_equal(engine, index_id, Value {std::int32_t {18}}).empty(), "UPDATE should remove old index key");
    require(find_index_equal(engine, index_id, Value {std::int32_t {19}}).size() == 1, "UPDATE should add new index key");

    auto delete_result = execute_ok(engine, "DELETE FROM users WHERE id = 1;");
    require(delete_result.affected_rows == 1, "indexed DELETE affected rows mismatch");
    require(find_index_equal(engine, index_id, Value {std::int32_t {19}}).empty(), "DELETE should remove index key");

    auto drop_index = execute_ok(engine, "DROP INDEX idx_age ON users;");
    require(drop_index.affected_rows == 1, "DROP INDEX affected rows mismatch");
    require(engine.catalog().find_index(collection->id(), "idx_age") == nullptr, "dropped index should leave catalog");
    require(!engine.index_manager().find_index(index_id).has_value(), "dropped index should leave manager");
}

void test_vector_distance_query()
{
    Engine engine;
    execute_ok(engine, "CREATE DATABASE vectors;");
    execute_ok(engine, "USE vectors;");
    execute_ok(engine, "CREATE COLLECTION docs (id BIGINT PRIMARY KEY, embedding VECTOR(3));");
    execute_ok(engine, "INSERT INTO docs VALUES (1, [0.0, 0.0, 0.0]);");
    execute_ok(engine, "INSERT INTO docs VALUES (2, [1.0, 0.0, 0.0]);");
    execute_ok(engine, "INSERT INTO docs VALUES (3, [0.2, 0.0, 0.0]);");

    auto result = execute_ok(
        engine,
        "SELECT id FROM docs ORDER BY l2_distance(embedding, [0.1, 0.0, 0.0]) ASC LIMIT 2;"
    );
    require(result.kind == ExecutionResultKind::RowSet, "vector SELECT result kind mismatch");
    require(result.rows.size() == 2, "vector SELECT row count mismatch");
    require(get_value<std::int64_t>(result.rows[0].values[0]) == 1, "vector nearest first mismatch");
    require(get_value<std::int64_t>(result.rows[1].values[0]) == 3, "vector nearest second mismatch");
}

void test_engine_error_mapping()
{
    Engine engine;

    auto parse_error = execute_error(engine, "SELECT FROM;");
    require(parse_error.code == EngineErrorCode::ParserError, "parser error code mismatch");

    auto binder_error = execute_error(engine, "SHOW COLLECTIONS;");
    require(binder_error.code == EngineErrorCode::BinderError, "binder error code mismatch");
}

void test_sessions_share_instance_but_keep_context()
{
    DatabaseInstance instance;
    Session first {instance};
    Session second {instance};

    auto create_database = first.execute_sql("CREATE DATABASE shared;");
    require(create_database.has_value(), "CREATE DATABASE should succeed");

    auto first_use = first.execute_sql("USE shared;");
    require(first_use.has_value(), "first USE should succeed");
    require(first.current_database_id().has_value(), "first session should select database");
    require(!second.current_database_id().has_value(), "second session should not inherit selected database");

    auto second_use = second.execute_sql("USE shared;");
    require(second_use.has_value(), "second USE should see shared database");
    require(second.current_database_id() == first.current_database_id(), "sessions should select same database id");
}

} // namespace

int main()
{
    try {
        test_execute_sql_end_to_end();
        test_vector_distance_query();
        test_engine_error_mapping();
        test_sessions_share_instance_but_keep_context();
    } catch (const std::exception & exception) {
        std::cerr << exception.what() << '\n';
        return 1;
    }

    return 0;
}

#include "core/engine/engine.hpp"
#include "core/engine/session.hpp"

#include <cstdint>
#include <exception>
#include <iostream>
#include <stdexcept>
#include <string>

namespace
{

using namespace litedb::core::engine;
using namespace litedb::core::executor;
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

    auto selected = execute_ok(engine, "SELECT name, age FROM users WHERE id = 1;");
    require(selected.kind == ExecutionResultKind::RowSet, "SELECT result kind mismatch");
    require(selected.columns.size() == 2, "SELECT column count mismatch");
    require(selected.rows.size() == 1, "SELECT row count mismatch");
    require(get_value<std::string>(selected.rows[0].values[0]) == "alice", "SELECT name mismatch");
    require(get_value<std::int32_t>(selected.rows[0].values[1]) == 18, "SELECT age mismatch");
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
        test_engine_error_mapping();
        test_sessions_share_instance_but_keep_context();
    } catch (const std::exception & exception) {
        std::cerr << exception.what() << '\n';
        return 1;
    }

    return 0;
}

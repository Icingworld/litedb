#include "client/client.hpp"
#include "server/server.hpp"

#include <asio.hpp>

#include <cstdint>
#include <exception>
#include <filesystem>
#include <iostream>
#include <memory>
#include <stdexcept>
#include <string>

namespace
{

using namespace litedb::client;
using namespace litedb::core::engine;
using namespace litedb::core::executor;
using namespace litedb::core::schema;
using namespace litedb::server;

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

asio::awaitable<void> run_client_flow(Server & server, bool & passed, std::string & failure)
{
    try {
        auto executor = co_await asio::this_coro::executor;
        auto & io = static_cast<asio::io_context &>(executor.context());
        Client client {io};

        auto connected = co_await client.connect("127.0.0.1", server.port());
        require(connected.has_value(), connected.has_value() ? "" : connected.error().message.c_str());

        auto ping = co_await client.ping();
        require(ping.has_value(), ping.has_value() ? "" : ping.error().message.c_str());

        auto create_database = co_await client.execute_sql("CREATE DATABASE demo;");
        require(create_database.has_value(), create_database.has_value() ? "" : create_database.error().message.c_str());

        auto use_database = co_await client.execute_sql("USE demo;");
        require(use_database.has_value(), use_database.has_value() ? "" : use_database.error().message.c_str());
        require(use_database->kind == ExecutionResultKind::UseDatabase, "USE result kind mismatch");

        auto create_collection = co_await client.execute_sql(
            "CREATE COLLECTION users (id BIGINT NOT NULL, name VARCHAR(64), age INTEGER);"
        );
        require(create_collection.has_value(), create_collection.has_value() ? "" : create_collection.error().message.c_str());

        auto insert = co_await client.execute_sql("INSERT INTO users VALUES (1, 'alice', 18);");
        require(insert.has_value(), insert.has_value() ? "" : insert.error().message.c_str());
        require(insert->affected_rows == 1, "INSERT affected rows mismatch");

        auto selected = co_await client.execute_sql("SELECT name, age FROM users WHERE id = 1;");
        require(selected.has_value(), selected.has_value() ? "" : selected.error().message.c_str());
        require(selected->kind == ExecutionResultKind::RowSet, "SELECT result kind mismatch");
        require(selected->rows.size() == 1, "SELECT row count mismatch");
        require(get_value<std::string>(selected->rows[0].values[0]) == "alice", "SELECT name mismatch");
        require(get_value<std::int32_t>(selected->rows[0].values[1]) == 18, "SELECT age mismatch");

        auto bad_sql = co_await client.execute_sql("SELECT FROM;");
        require(!bad_sql.has_value(), "bad SQL should fail");
        require(bad_sql.error().code == ClientErrorCode::ServerError, "bad SQL error code mismatch");

        client.close();
        server.close();
        passed = true;
    } catch (const std::exception & exception) {
        failure = exception.what();
        server.close();
    }
}

asio::awaitable<void> run_persistent_write_flow(Server & server, bool & passed, std::string & failure)
{
    try {
        auto executor = co_await asio::this_coro::executor;
        auto & io = static_cast<asio::io_context &>(executor.context());
        Client client {io};

        auto connected = co_await client.connect("127.0.0.1", server.port());
        require(connected.has_value(), connected.has_value() ? "" : connected.error().message.c_str());
        require((co_await client.execute_sql("CREATE DATABASE demo;")).has_value(), "CREATE DATABASE should succeed");
        require((co_await client.execute_sql("USE demo;")).has_value(), "USE should succeed");
        require((co_await client.execute_sql("CREATE COLLECTION users (id BIGINT NOT NULL, name VARCHAR(64));")).has_value(), "CREATE COLLECTION should succeed");
        require((co_await client.execute_sql("INSERT INTO users VALUES (1, 'persisted');")).has_value(), "INSERT should succeed");

        client.close();
        server.close();
        passed = true;
    } catch (const std::exception & exception) {
        failure = exception.what();
        server.close();
    }
}

asio::awaitable<void> run_persistent_read_flow(Server & server, bool & passed, std::string & failure)
{
    try {
        auto executor = co_await asio::this_coro::executor;
        auto & io = static_cast<asio::io_context &>(executor.context());
        Client client {io};

        auto connected = co_await client.connect("127.0.0.1", server.port());
        require(connected.has_value(), connected.has_value() ? "" : connected.error().message.c_str());
        require((co_await client.execute_sql("USE demo;")).has_value(), "USE after reopen should succeed");
        auto selected = co_await client.execute_sql("SELECT name FROM users WHERE id = 1;");
        require(selected.has_value(), selected.has_value() ? "" : selected.error().message.c_str());
        require(selected->rows.size() == 1, "persistent server row count mismatch");
        require(get_value<std::string>(selected->rows[0].values[0]) == "persisted", "persistent server value mismatch");

        client.close();
        server.close();
        passed = true;
    } catch (const std::exception & exception) {
        failure = exception.what();
        server.close();
    }
}

void test_client_server_execute_sql()
{
    asio::io_context io;
    auto instance = std::make_shared<DatabaseInstance>();
    Server server {
        io,
        ServerConfig {.host = "127.0.0.1", .port = 0},
        instance,
    };

    bool passed = false;
    std::string failure;
    asio::co_spawn(io, server.listen(), asio::detached);
    asio::co_spawn(io, run_client_flow(server, passed, failure), asio::detached);
    io.run();

    if (!failure.empty()) {
        throw std::runtime_error(failure);
    }
    require(passed, "client/server flow did not complete");
}

void test_client_server_persistent_reopen()
{
    const auto data_dir = std::filesystem::temp_directory_path() / "litedb_client_server_persistence_test";
    std::filesystem::remove_all(data_dir);

    {
        asio::io_context io;
        auto instance = std::make_shared<DatabaseInstance>(DatabaseConfig {.data_dir = data_dir});
        Server server {
            io,
            ServerConfig {.host = "127.0.0.1", .port = 0},
            instance,
        };

        bool passed = false;
        std::string failure;
        asio::co_spawn(io, server.listen(), asio::detached);
        asio::co_spawn(io, run_persistent_write_flow(server, passed, failure), asio::detached);
        io.run();
        if (!failure.empty()) {
            throw std::runtime_error(failure);
        }
        require(passed, "persistent write flow did not complete");
    }

    {
        asio::io_context io;
        auto instance = std::make_shared<DatabaseInstance>(DatabaseConfig {.data_dir = data_dir});
        Server server {
            io,
            ServerConfig {.host = "127.0.0.1", .port = 0},
            instance,
        };

        bool passed = false;
        std::string failure;
        asio::co_spawn(io, server.listen(), asio::detached);
        asio::co_spawn(io, run_persistent_read_flow(server, passed, failure), asio::detached);
        io.run();
        if (!failure.empty()) {
            throw std::runtime_error(failure);
        }
        require(passed, "persistent read flow did not complete");
    }
}

} // namespace

int main()
{
    try {
        test_client_server_execute_sql();
        test_client_server_persistent_reopen();
    } catch (const std::exception & exception) {
        std::cerr << exception.what() << '\n';
        return 1;
    }

    return 0;
}

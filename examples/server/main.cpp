#include "core/database/database_engine.hpp"
#include "server/server.hpp"

#include <asio.hpp>

#include <cstdint>
#include <exception>
#include <filesystem>
#include <iostream>
#include <memory>
#include <stdexcept>
#include <string>
#include <string_view>
#include <utility>

namespace
{

struct Options
{
    std::string host {"127.0.0.1"};
    std::uint16_t port {5252};
    std::filesystem::path data_dir {"litedb-data"};
};

[[nodiscard]]
std::uint16_t parse_port(std::string_view value)
{
    std::size_t consumed {0};
    const auto parsed = std::stoul(std::string(value), &consumed, 10);
    if (consumed != value.size() || parsed > 65535) {
        throw std::runtime_error("invalid port: " + std::string(value));
    }
    return static_cast<std::uint16_t>(parsed);
}

[[nodiscard]]
Options parse_options(int argc, char ** argv)
{
    Options options;
    for (int index = 1; index < argc; ++index) {
        const std::string_view arg {argv[index]};
        if (arg == "--host") {
            if (++index >= argc) {
                throw std::runtime_error("--host requires a value");
            }
            options.host = argv[index];
        } else if (arg == "--port") {
            if (++index >= argc) {
                throw std::runtime_error("--port requires a value");
            }
            options.port = parse_port(argv[index]);
        } else if (arg == "--data-dir") {
            if (++index >= argc) {
                throw std::runtime_error("--data-dir requires a value");
            }
            options.data_dir = std::filesystem::path {argv[index]};
        } else if (arg == "--help" || arg == "-h") {
            std::cout << "usage: litedb_example_server [--host HOST] [--port PORT] [--data-dir PATH]\n";
            std::exit(0);
        } else {
            throw std::runtime_error("unknown option: " + std::string(arg));
        }
    }
    return options;
}

} // namespace

int main(int argc, char ** argv)
{
    try {
        const auto options = parse_options(argc, argv);

        asio::io_context io;
        auto opened = litedb::core::database::DatabaseEngine::open(
            litedb::core::database::DatabaseConfig {.data_dir = options.data_dir}
        );
        if (!opened.has_value()) {
            std::cerr << "error: " << opened.error().message() << '\n';
            return 1;
        }
        std::shared_ptr<litedb::core::database::DatabaseEngine> engine {std::move(*opened)};
        litedb::server::Server server {
            io,
            litedb::server::ServerConfig {
                .host = options.host,
                .port = options.port,
            },
            engine,
        };

        asio::signal_set signals {io, SIGINT, SIGTERM};
        signals.async_wait(
            [&](const std::error_code &, int) {
                server.close();
            }
        );

        std::cout << "LiteDB example server listening on " << options.host << ':' << server.port() << '\n';
        asio::co_spawn(io, server.listen(), asio::detached);
        io.run();
        std::cout << "LiteDB example server stopped\n";
    } catch (const std::exception & exception) {
        std::cerr << "error: " << exception.what() << '\n';
        return 1;
    }

    return 0;
}

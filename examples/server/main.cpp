#include "core/engine/database_instance.hpp"
#include "server/server.hpp"

#include <asio.hpp>

#include <cstdint>
#include <exception>
#include <iostream>
#include <memory>
#include <stdexcept>
#include <string>
#include <string_view>

namespace
{

struct Options
{
    std::string host {"127.0.0.1"};
    std::uint16_t port {5252};
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
        } else if (arg == "--help" || arg == "-h") {
            std::cout << "usage: litedb_example_server [--host HOST] [--port PORT]\n";
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
        auto instance = std::make_shared<litedb::core::engine::DatabaseInstance>();
        litedb::server::Server server {
            io,
            litedb::server::ServerConfig {
                .host = options.host,
                .port = options.port,
            },
            instance,
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

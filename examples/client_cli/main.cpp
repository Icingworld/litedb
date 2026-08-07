#include "client/client.hpp"

#include <asio.hpp>

#include <algorithm>
#include <cstdint>
#include <exception>
#include <iomanip>
#include <iostream>
#include <optional>
#include <sstream>
#include <stdexcept>
#include <string>
#include <string_view>
#include <vector>

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
            std::cout << "usage: litedb_example_client_cli [--host HOST] [--port PORT]\n";
            std::exit(0);
        } else {
            throw std::runtime_error("unknown option: " + std::string(arg));
        }
    }
    return options;
}

[[nodiscard]]
std::string trim(std::string_view value)
{
    const auto first = value.find_first_not_of(" \t\r\n");
    if (first == std::string_view::npos) {
        return {};
    }
    const auto last = value.find_last_not_of(" \t\r\n");
    return std::string(value.substr(first, last - first + 1));
}

[[nodiscard]]
std::string format_value(const litedb::core::common::Value & value)
{
    using litedb::core::common::NullValue;
    using litedb::core::common::VectorValue;

    return std::visit(
        [](const auto & data) -> std::string {
            using T = std::decay_t<decltype(data)>;
            if constexpr (std::is_same_v<T, NullValue>) {
                return "NULL";
            } else if constexpr (std::is_same_v<T, bool>) {
                return data ? "true" : "false";
            } else if constexpr (std::is_same_v<T, std::string>) {
                return data;
            } else if constexpr (std::is_same_v<T, VectorValue>) {
                std::ostringstream out;
                out << '[';
                for (std::size_t index = 0; index < data.size(); ++index) {
                    if (index != 0) {
                        out << ", ";
                    }
                    out << data[index];
                }
                out << ']';
                return out.str();
            } else {
                return std::to_string(data);
            }
        },
        value.data()
    );
}

void print_separator(const std::vector<std::size_t> & widths)
{
    std::cout << '+';
    for (const auto width : widths) {
        std::cout << std::string(width + 2, '-') << '+';
    }
    std::cout << '\n';
}

void print_row(const std::vector<std::string> & values, const std::vector<std::size_t> & widths)
{
    std::cout << '|';
    for (std::size_t index = 0; index < widths.size(); ++index) {
        const auto value = index < values.size() ? values[index] : std::string {};
        std::cout << ' ' << std::left << std::setw(static_cast<int>(widths[index])) << value << " |";
    }
    std::cout << '\n';
}

void print_result(const litedb::core::executor::ExecutionResult & result)
{
    using litedb::core::executor::ExecutionResultKind;

    if (result.kind == ExecutionResultKind::UseDatabase) {
        if (result.selected_database_name.has_value()) {
            std::cout << "using database " << result.selected_database_name.value() << '\n';
        } else {
            std::cout << "database selected\n";
        }
        return;
    }

    if (result.kind == ExecutionResultKind::Command) {
        std::cout << "OK, affected rows: " << result.affected_rows << '\n';
        return;
    }

    std::vector<std::string> headers;
    std::vector<std::size_t> widths;
    headers.reserve(result.columns.size());
    widths.reserve(result.columns.size());
    for (const auto & column : result.columns) {
        headers.push_back(column.name);
        widths.push_back(column.name.size());
    }

    std::vector<std::vector<std::string>> rows;
    rows.reserve(result.rows.size());
    for (const auto & row : result.rows) {
        std::vector<std::string> values;
        values.reserve(row.values.size());
        for (std::size_t index = 0; index < row.values.size(); ++index) {
            auto text = format_value(row.values[index]);
            if (index < widths.size()) {
                widths[index] = std::max(widths[index], text.size());
            }
            values.push_back(std::move(text));
        }
        rows.push_back(std::move(values));
    }

    print_separator(widths);
    print_row(headers, widths);
    print_separator(widths);
    for (const auto & row : rows) {
        print_row(row, widths);
    }
    print_separator(widths);
    std::cout << result.rows.size() << " row(s)\n";
}

void print_client_error(const litedb::client::ClientError & error)
{
    std::cerr << "error: " << error.message();
    const auto * context = error.context<litedb::client::ClientErrorContext>();
    if (context != nullptr) {
        if (context->server_code != 0) {
            std::cerr << " (server code " << context->server_code << ')';
        }
        if (context->error) {
            std::cerr << " (native " << context->error << ')';
        }
    }
    std::cerr << '\n';
}

asio::awaitable<void> run_repl(const Options options, int & exit_code)
{
    auto executor = co_await asio::this_coro::executor;
    auto & io = static_cast<asio::io_context &>(executor.context());
    litedb::client::Client client {io};

    auto connected = co_await client.connect(options.host, options.port);
    if (!connected.has_value()) {
        print_client_error(connected.error());
        exit_code = 1;
        co_return;
    }

    auto ping = co_await client.ping();
    if (!ping.has_value()) {
        print_client_error(ping.error());
        exit_code = 1;
        co_return;
    }

    std::cout << "Connected to LiteDB at " << options.host << ':' << options.port << '\n';

    std::string pending;
    std::string line;
    for (;;) {
        std::cout << (pending.empty() ? "litedb> " : "   ...> ");
        if (!std::getline(std::cin, line)) {
            break;
        }

        const auto command = trim(line);
        if (pending.empty() && (command == ".exit" || command == ".quit")) {
            break;
        }
        if (command.empty()) {
            continue;
        }

        if (!pending.empty()) {
            pending.push_back('\n');
        }
        pending += line;

        if (line.find(';') == std::string::npos) {
            continue;
        }

        auto sql = trim(pending);
        pending.clear();
        if (sql.empty()) {
            continue;
        }

        auto result = co_await client.execute_sql(sql);
        if (!result.has_value()) {
            print_client_error(result.error());
            continue;
        }
        print_result(*result);
    }

    auto closed = co_await client.close();
    if (!closed.has_value()) {
        print_client_error(closed.error());
        exit_code = 1;
        co_return;
    }
    exit_code = 0;
}

} // namespace

int main(int argc, char ** argv)
{
    try {
        const auto options = parse_options(argc, argv);
        asio::io_context io;
        int exit_code {0};
        asio::co_spawn(io, run_repl(options, exit_code), asio::detached);
        io.run();
        return exit_code;
    } catch (const std::exception & exception) {
        std::cerr << "error: " << exception.what() << '\n';
        return 1;
    }
}

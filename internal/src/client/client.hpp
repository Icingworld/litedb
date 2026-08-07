#pragma once

#include <cstdint>
#include <expected>
#include <string_view>

#include <asio.hpp>

#include "core/executor/execution_result.hpp"
#include "client/client_error.hpp"
#include "protocol/frame.hpp"

namespace litedb::client
{

/**
 * @brief 客户端
 */
class Client
{
public:
    explicit Client(asio::io_context & io);

    Client(const Client &) = delete;

    Client & operator=(const Client &) = delete;

public:
    /**
     * @brief 连接到服务器
     * @param host 主机地址
     * @param port 端口号
     * @return 是否成功
     */
    [[nodiscard]]
    asio::awaitable<std::expected<void, ClientError>> connect(
        std::string_view host,
        std::uint16_t port
    );

    /**
     * @brief 发送 Ping 请求
     * @return 是否成功
     */
    [[nodiscard]]
    asio::awaitable<std::expected<void, ClientError>> ping();

    /**
     * @brief 执行 SQL 语句
     * @param sql SQL 语句
     * @return 执行结果
     */
    [[nodiscard]]
    asio::awaitable<std::expected<core::executor::ExecutionResult, ClientError>>
    execute_sql(
        std::string_view sql
    );

    /**
     * @brief 关闭客户端
     * @return 是否成功
     */
    [[nodiscard]]
    asio::awaitable<std::expected<void, ClientError>> close();

private:
    /**
     * @brief 获取下一个请求 ID
     * @return 下一个请求 ID
     */
    [[nodiscard]]
    std::uint64_t next_request_id() noexcept;

    /**
     * @brief 发送并接收帧
     * @param frame 帧
     * @return 接收到的帧
     */
    [[nodiscard]]
    asio::awaitable<std::expected<protocol::Frame, ClientError>> roundtrip(
        protocol::Frame frame
    );

private:
    asio::ip::tcp::socket socket_;                  ///< 套接字
    std::uint64_t next_request_id_ {1};             ///< 下一个请求 ID
};

} // namespace litedb::client

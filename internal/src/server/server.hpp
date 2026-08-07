#pragma once

#include <cstddef>
#include <cstdint>
#include <expected>
#include <memory>
#include <string>

#include <asio.hpp>

#include "core/database/database_engine.hpp"
#include "protocol/frame.hpp"
#include "protocol/message.hpp"

namespace litedb::server
{

/**
 * @brief 服务端配置
 */
struct ServerConfig
{
    std::string host {"127.0.0.1"};                           // 主机地址
    std::uint16_t port {0};                                   // 端口号
    std::size_t max_frame_size {protocol::MaxFrameSize};      // 最大帧大小
    protocol::ProtocolDecodeLimits decode_limits {};          // 解码限制
};

/**
 * @brief 服务端
 */
class Server
{
public:
    Server(
        asio::io_context & io,
        ServerConfig config,
        std::shared_ptr<core::database::DatabaseEngine> engine
    );

    Server(const Server &) = delete;

    Server & operator=(const Server &) = delete;

public:
    /**
     * @brief 获取端口号
     * @return 端口号
     */
    [[nodiscard]]
    std::uint16_t port() const;

    /**
     * @brief 关闭服务器
     */
    void close();

    /**
     * @brief 监听
     * @return 是否成功
     */
    [[nodiscard]]
    asio::awaitable<void> listen();

private:
    /**
     * @brief 处理连接
     * @param socket 套接字
     * @return 是否成功
     */
    [[nodiscard]]
    asio::awaitable<void> handle_connection(asio::ip::tcp::socket socket);

    /**
     * @brief 构建错误响应
     * @param request_id 请求 ID
     * @param code 错误码
     * @param message 错误消息
     * @return 错误响应
     */
    [[nodiscard]]
    std::expected<protocol::Frame, protocol::ProtocolError>
    make_error_response(
        std::uint64_t request_id,
        std::uint16_t code,
        std::string message
    ) const;

private:
    asio::ip::tcp::acceptor acceptor_;                            // 套接字接受器
    ServerConfig config_;                                         // 配置
    std::shared_ptr<core::database::DatabaseEngine> engine_;      // 数据库引擎
};

} // namespace litedb::server

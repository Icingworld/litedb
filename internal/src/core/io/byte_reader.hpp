#pragma once

#include <cstddef>
#include <expected>
#include <span>

#include "core/io/io_error.hpp"

namespace litedb::core::io
{

/**
 * @brief 字节读取器
 */
class ByteReader
{
public:
    virtual ~ByteReader() = default;

public:
    /**
     * @brief 尽力读取字节数据
     * @param data 字节数据
     * @return 结果
     * @note 允许短读，返回的字节数可能小于请求的字节数
     */
    virtual std::expected<std::size_t, IoError> read_some(std::span<std::byte> data) = 0;

    /**
     * @brief 读取精确的字节数据
     * @param data 字节数据
     * @return 结果
     * @note 必须把缓冲区填满，如果读取的字节数不足，则返回错误
     */
    std::expected<void, IoError> read_exact(std::span<std::byte> data);
};

} // namespace litedb::core::io

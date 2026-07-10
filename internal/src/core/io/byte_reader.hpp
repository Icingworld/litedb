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
     * @brief 读取字节数据
     * @param data 字节数据
     * @return 结果
     */
    virtual std::expected<std::size_t, IoError> read_bytes(std::span<std::byte> data) = 0;
};

} // namespace litedb::core::io

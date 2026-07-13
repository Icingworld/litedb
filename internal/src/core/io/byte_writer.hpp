#pragma once

#include <cstddef>
#include <expected>
#include <span>

#include "core/io/io_error.hpp"

namespace litedb::core::io
{

/**
 * @brief 字节写入器
 */
class ByteWriter
{
public:
    virtual ~ByteWriter() = default;

public:
    /**
     * @brief 写入字节数据
     * @param data 字节数据
     * @return 结果
     */
    virtual std::expected<void, IoError> write_bytes(std::span<const std::byte> data) = 0;
};

} // namespace litedb::core::io

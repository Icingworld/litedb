#pragma once

#include <cstdint>
#include <span>

#include "core/io/byte_reader.hpp"

namespace litedb::core::io
{

/**
 * @brief 内存缓冲区字节读取器
 */
class BufferByteReader final : public ByteReader
{
public:
    explicit BufferByteReader(std::span<const std::byte> data) noexcept;

public:
    /**
     * @brief 读取字节数据
     * @param data 字节数据
     * @return 结果
     */
    std::expected<std::size_t, IoError> read_bytes(std::span<std::byte> data) override;

private:
    std::span<const std::byte> data_;     ///< 字节数据
    std::uint64_t offset_;                ///< 文件偏移
};

} // namespace litedb::core::io

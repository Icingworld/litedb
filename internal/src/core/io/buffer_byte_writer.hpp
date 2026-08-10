#pragma once

#include <cstddef>
#include <span>
#include <vector>

#include "core/io/byte_writer.hpp"

namespace litedb::core::io
{

// 内存缓冲区字节写入器
class BufferByteWriter final : public ByteWriter
{
public:
    // 构造一个拥有最大缓冲区大小的字节写入器
    explicit BufferByteWriter(std::size_t max_bytes) noexcept;

public:
    // 写入字节数据
    [[nodiscard]]
    std::expected<void, IoError> write_bytes(std::span<const std::byte> data) override;

    // 获取字节数据
    [[nodiscard]]
    const std::vector<std::byte> & bytes() const noexcept;

    // 获取字节数据所有权
    // 调用后缓冲区将不再拥有字节数据
    [[nodiscard]]
    std::vector<std::byte> take_bytes() noexcept;

private:
    std::vector<std::byte> bytes_;
    std::size_t max_bytes_;
};

} // namespace litedb::core::io

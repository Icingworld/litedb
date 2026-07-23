#pragma once

#include <cstddef>
#include <span>
#include <vector>

#include "core/io/byte_writer.hpp"

namespace litedb::core::io
{

/**
 * @brief 内存缓冲区字节写入器
 */
class BufferByteWriter final : public ByteWriter
{
public:
    explicit BufferByteWriter(std::size_t max_bytes) noexcept;

public:
    /**
     * @brief 写入字节数据
     * @param data 字节数据
     * @return 结果
     */
    [[nodiscard]]
    std::expected<void, IoError> write_bytes(std::span<const std::byte> data) override;

    /**
     * @brief 获取字节数据
     * @return 字节数据
     */
    [[nodiscard]]
    const std::vector<std::byte> & bytes() const noexcept;

    /**
     * @brief 获取字节数据
     * @return 字节数据
     * @note 该方法会获取字节数据的所有权，调用后缓冲区将不再保证拥有字节数据
     */
    [[nodiscard]]
    std::vector<std::byte> take_bytes() noexcept;    

private:
    std::vector<std::byte> bytes_;     ///< 字节数据
    std::size_t max_bytes_;            ///< 最大缓冲区大小
};

} // namespace litedb::core::io

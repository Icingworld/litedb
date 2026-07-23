#pragma once

#include "core/io/byte_reader.hpp"
#include "core/filesystem/file_handle.hpp"

namespace litedb::core::io
{

/**
 * @brief 基于文件偏移的字节读取器
 * @note 借用传入的 FileHandle，读取器不得比文件句柄存活更久。
 * @note 逻辑偏移仅支持单消费者访问，不保证多线程共享安全。
 */
class FileByteReader final : public ByteReader
{
public:
    explicit FileByteReader(filesystem::FileHandle & file) noexcept;

public:
    /**
     * @brief 读取字节数据
     * @param data 字节数据
     * @return 结果
     */
    [[nodiscard]]
    std::expected<std::size_t, IoError> read_some(std::span<std::byte> data) override;

    /**
     * @brief 获取文件偏移
     * @return 文件偏移
     */
    [[nodiscard]]
    std::uint64_t offset() const noexcept;

private:
    filesystem::FileHandle * file_;     ///< 文件句柄
    std::uint64_t offset_;              ///< 文件偏移
};

} // namespace litedb::core::io

#pragma once

#include <cstdint>

#include "core/filesystem/file_handle.hpp"
#include "core/io/byte_writer.hpp"

namespace litedb::core::io
{

/**
 * @brief 基于文件偏移的字节写入器
 * @note 借用传入的 FileHandle，写入器不得比文件句柄存活更久。
 * @note 逻辑偏移仅支持单消费者访问，不保证多线程共享安全。
 */
class FileByteWriter final : public ByteWriter
{
public:
    explicit FileByteWriter(filesystem::FileHandle & file, std::uint64_t offset = 0) noexcept;

public:
    /**
     * @brief 写入字节数据
     * @param data 字节数据
     * @return 结果
     */
    [[nodiscard]]
    std::expected<void, IoError> write_bytes(std::span<const std::byte> data) override;

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

/**
 * @brief 基于文件追加语义的字节写入器
 * @note 借用传入的 FileHandle，追加器不得比文件句柄存活更久。
 */
class FileByteAppender final : public ByteWriter
{
public:
    explicit FileByteAppender(filesystem::FileHandle & file) noexcept;

public:
    /**
     * @brief 写入字节数据
     * @param data 字节数据
     * @return 结果
     */
    [[nodiscard]]
    std::expected<void, IoError> write_bytes(std::span<const std::byte> data) override;

private:
    filesystem::FileHandle * file_;     ///< 文件句柄
};

} // namespace litedb::core::io

#pragma once

#include <cstddef>
#include <cstdint>
#include <expected>
#include <span>

#include "core/filesystem/filesystem_error.hpp"

namespace litedb::core::filesystem::backend
{

/**
 * @brief 文件句柄后端
 */
class FileHandleBackend
{
public:
    virtual ~FileHandleBackend() = default;

public:
    /**
     * @brief 关闭文件
     * @return 结果
     */
    virtual std::expected<void, FileSystemError> close() = 0;

    /**
     * @brief 从指定偏移读取数据
     * @param offset 偏移量
     * @param buffer 缓冲区
     * @return 实际读取的字节数；到达文件末尾时允许小于缓冲区大小
     */
    virtual std::expected<std::size_t, FileSystemError> read_at(
        std::uint64_t offset,
        std::span<std::byte> buffer
    ) = 0;

    /**
     * @brief 从指定偏移写入数据
     * @param offset 偏移量
     * @param data 数据
     * @return 结果
     */
    virtual std::expected<void, FileSystemError> write_at(
        std::uint64_t offset,
        std::span<const std::byte> data
    ) = 0;

    /**
     * @brief 追加数据
     * @param data 数据
     * @return 结果
     */
    virtual std::expected<void, FileSystemError> append(std::span<const std::byte> data) = 0;

    /**
     * @brief 获取文件大小
     * @return 文件大小
     */
    virtual std::expected<std::uint64_t, FileSystemError> size() = 0;

    /**
     * @brief 截断文件
     * @param size 大小
     * @return 结果
     */
    virtual std::expected<void, FileSystemError> truncate(std::uint64_t size) = 0;

    /**
     * @brief 同步数据
     * @return 结果
     */
    virtual std::expected<void, FileSystemError> sync_data() = 0;

    /**
     * @brief 同步所有
     * @return 结果
     */
    virtual std::expected<void, FileSystemError> sync_all() = 0;
};

} // namespace litedb::core::filesystem::backend

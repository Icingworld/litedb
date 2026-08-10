#pragma once

#include <cstddef>
#include <cstdint>
#include <expected>
#include <span>

#include "core/filesystem/filesystem_error.hpp"

namespace litedb::core::filesystem::backend
{

// 文件句柄后端
class FileHandleBackend
{
public:
    virtual ~FileHandleBackend() = default;

public:
    // 关闭文件
    virtual std::expected<void, FileSystemError> close() = 0;

    // 从指定偏移读取数据
    // 返回实际读取的字节数；到达文件末尾时允许小于缓冲区大小
    virtual std::expected<std::size_t, FileSystemError> read_at(
        std::uint64_t offset,
        std::span<std::byte> buffer
    ) = 0;

    // 从指定偏移写入全部数据
    // 成功时保证整个缓冲区均已写入，否则返回错误
    virtual std::expected<void, FileSystemError> write_at(
        std::uint64_t offset,
        std::span<const std::byte> data
    ) = 0;

    // 将数据追加到文件末尾
    // 同一 FileHandle 实例上的操作会被串行化，不会因查询文件大小与写入之间的竞态而互相覆盖
    // 不同 FileHandle 或进程之间不提供原子追加保证
    virtual std::expected<void, FileSystemError> append(std::span<const std::byte> data) = 0;

    // 获取当前文件大小
    virtual std::expected<std::uint64_t, FileSystemError> size() = 0;

    // 将文件调整到指定大小
    virtual std::expected<void, FileSystemError> truncate(std::uint64_t size) = 0;

    // 将文件数据及读取数据所必需的元数据同步到持久化存储
    virtual std::expected<void, FileSystemError> sync_data() = 0;

    // 将文件数据及文件的全部元数据同步到持久化存储
    virtual std::expected<void, FileSystemError> sync_all() = 0;
};

} // namespace litedb::core::filesystem::backend

#pragma once

#include <cstddef>
#include <cstdint>
#include <expected>
#include <memory>
#include <span>

#include "core/filesystem/filesystem_error.hpp"

namespace litedb::core::filesystem
{

namespace backend
{

class FileHandleBackend;

} // namespace backend

/**
 * @brief 文件句柄
 */
class FileHandle
{
public:
    explicit FileHandle(std::unique_ptr<backend::FileHandleBackend> backend);

    FileHandle(const FileHandle &) = delete;
    
    FileHandle & operator=(const FileHandle &) = delete;

    FileHandle(FileHandle &&) noexcept;
    
    FileHandle & operator=(FileHandle &&) noexcept;

    ~FileHandle();    

public:
    /**
     * @brief 从指定偏移读取数据
     * @return 实际读取的字节数；到达文件末尾时允许小于缓冲区大小
     */
    [[nodiscard]]
    std::expected<std::size_t, FileSystemError> read_at(
        std::uint64_t offset,
        std::span<std::byte> buffer
    );

    /**
     * @brief 从指定偏移写入全部数据
     *
     * 成功时保证整个缓冲区均已写入，否则返回错误。
     */
    [[nodiscard]]
    std::expected<void, FileSystemError> write_at(
        std::uint64_t offset,
        std::span<const std::byte> data
    );

    /**
     * @brief 将数据原子地追加到文件末尾
     *
     * “原子”指同一文件的并发追加不会因查询文件大小与写入之间的竞态而互相覆盖。
     * 单次追加是否可能被拆分，由具体文件系统实现说明。
     */
    [[nodiscard]]
    std::expected<void, FileSystemError> append(std::span<const std::byte> data);

    /**
     * @brief 获取当前文件大小
     */
    [[nodiscard]]
    std::expected<std::uint64_t, FileSystemError> size();

    /**
     * @brief 将文件调整到指定大小
     */
    [[nodiscard]]
    std::expected<void, FileSystemError> truncate(std::uint64_t size);

    /**
     * @brief 将文件数据及读取数据所必需的元数据同步到持久化存储
     */
    [[nodiscard]]
    std::expected<void, FileSystemError> sync_data();

    /**
     * @brief 将文件数据及文件的全部元数据同步到持久化存储
     */
    [[nodiscard]]
    std::expected<void, FileSystemError> sync_all();

private:
    std::unique_ptr<backend::FileHandleBackend> backend_;   ///< 文件句柄后端
};

} // namespace litedb::core::filesystem

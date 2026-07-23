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
     * @brief 关闭文件
     * @return 结果
     */
    [[nodiscard]]
    std::expected<void, FileSystemError> close();

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
     * @brief 将数据追加到文件末尾
     *
     * 同一 FileHandle 实例上的操作会被串行化，不会因查询文件大小与写入之间的
     * 竞态而互相覆盖。不同 FileHandle 或进程之间不提供原子追加保证。
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

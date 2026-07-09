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
    virtual std::expected<std::size_t, FileSystemError> read_at(
        std::uint64_t offset,
        std::span<std::byte> buffer
    ) = 0;

    virtual std::expected<void, FileSystemError> write_at(
        std::uint64_t offset,
        std::span<const std::byte> data
    ) = 0;

    virtual std::expected<void, FileSystemError> append(std::span<const std::byte> data) = 0;

    virtual std::expected<std::uint64_t, FileSystemError> size() = 0;

    virtual std::expected<void, FileSystemError> truncate(std::uint64_t size) = 0;

    virtual std::expected<void, FileSystemError> sync_data() = 0;

    virtual std::expected<void, FileSystemError> sync_all() = 0;
};

} // namespace litedb::core::filesystem::backend

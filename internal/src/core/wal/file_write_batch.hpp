#pragma once

#include <expected>
#include <filesystem>
#include <span>
#include <vector>

#include "core/filesystem/filesystem.hpp"
#include "core/wal/wal_error.hpp"
#include "core/wal/wal_types.hpp"

namespace litedb::core::wal
{

class FileWriteBatch final
{
public:
    void add(FileWrite write);
    [[nodiscard]] const std::vector<FileWrite> & writes() const noexcept { return writes_; }
    [[nodiscard]] bool empty() const noexcept { return writes_.empty(); }

    [[nodiscard]] std::expected<std::vector<std::byte>, WalError> read(
        const FileTarget & target,
        std::uint64_t offset,
        std::span<const std::byte> base
    ) const;

    [[nodiscard]] std::expected<void, WalError> apply(
        const std::filesystem::path & data_directory,
        filesystem::FileSystem & filesystem,
        bool sync
    ) const;

    [[nodiscard]] static std::filesystem::path resolve_target(
        const std::filesystem::path & data_directory,
        const FileTarget & target
    );

private:
    std::vector<FileWrite> writes_;
};

} // namespace litedb::core::wal

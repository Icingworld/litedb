#pragma once

#include <expected>
#include <filesystem>
#include <optional>
#include <span>

#include "core/filesystem/file_handle.hpp"
#include "core/filesystem/filesystem.hpp"
#include "core/wal/wal_codec.hpp"

namespace litedb::core::wal
{

class WalStore final
{
public:
    WalStore(const WalStore &) = delete;
    WalStore & operator=(const WalStore &) = delete;
    WalStore(WalStore &&) noexcept = default;
    WalStore & operator=(WalStore &&) noexcept = default;

    [[nodiscard]] static std::expected<WalStore, WalError> open(
        std::filesystem::path path,
        filesystem::FileSystem & filesystem
    );

    [[nodiscard]] std::expected<transaction::Lsn, WalError> append_begin(
        transaction::TransactionId transaction_id
    );
    [[nodiscard]] std::expected<transaction::Lsn, WalError> append_write(
        transaction::TransactionId transaction_id,
        const FileWrite & write
    );
    [[nodiscard]] std::expected<transaction::Lsn, WalError> append_commit(
        transaction::TransactionId transaction_id
    );
    [[nodiscard]] std::expected<void, WalError> flush_through(transaction::Lsn lsn);
    [[nodiscard]] std::expected<WalScanResult, WalError> scan(bool truncate_incomplete_tail = true);
    [[nodiscard]] std::expected<void, WalError> truncate_tail(std::uint64_t valid_size);

    [[nodiscard]] const std::filesystem::path & path() const noexcept { return path_; }
    [[nodiscard]] std::optional<transaction::Lsn> flushed_lsn() const noexcept { return flushed_lsn_; }

private:
    WalStore(std::filesystem::path path, filesystem::FileHandle file) noexcept
        : path_(std::move(path)), file_(std::move(file))
    {
    }

    [[nodiscard]] std::expected<transaction::Lsn, WalError> append(
        WalRecordType type,
        transaction::TransactionId transaction_id,
        std::span<const std::byte> payload
    );

private:
    std::filesystem::path path_;
    filesystem::FileHandle file_;
    std::optional<transaction::Lsn> flushed_lsn_;
};

} // namespace litedb::core::wal

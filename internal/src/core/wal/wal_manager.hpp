#pragma once

#include <expected>
#include <filesystem>
#include <functional>
#include <span>

#include "core/filesystem/filesystem.hpp"
#include "core/wal/wal_store.hpp"

namespace litedb::core::wal
{

struct WalManagerMetrics
{
    std::uint64_t generation {0};
    transaction::TransactionId checkpoint_transaction_id {transaction::InvalidTransactionId};
    std::uint64_t size_bytes {0};
    std::size_t retained_segments {0};
};

enum class WalRotationStage
{
    AfterTemporarySync,
    AfterPublish,
    AfterDirectorySync,
    AfterSwitch,
    AfterOldSegmentRemoval,
};

using WalRotationHook = std::function<void(WalRotationStage)>;

/**
 * @brief 管理 WAL 分段的发现、当前段与轮换
 */
class WalManager final
{
private:
    WalManager(
        std::filesystem::path directory,
        filesystem::FileSystem & filesystem,
        WalStore active,
        std::size_t retained_segments,
        WalDecodeLimits limits
    ) noexcept;

public:
    WalManager(const WalManager &) = delete;
    WalManager & operator=(const WalManager &) = delete;
    WalManager(WalManager &&) noexcept = default;
    WalManager & operator=(WalManager &&) noexcept = default;

    [[nodiscard]]
    static std::expected<WalManager, WalError> open(
        std::filesystem::path directory,
        filesystem::FileSystem & filesystem,
        WalDecodeLimits limits = {}
    );

    /**
     * @brief 在追加 Begin/FileWrite/Commit 前验证整个事务是否仍可由相同预算恢复
     */
    [[nodiscard]]
    std::expected<void, WalError> validate_transaction(
        std::span<const FileWrite> writes
    ) const;

    [[nodiscard]]
    std::expected<transaction::Lsn, WalError> append_begin(transaction::TransactionId transaction_id);

    [[nodiscard]]
    std::expected<transaction::Lsn, WalError> append_write(
        transaction::TransactionId transaction_id,
        const FileWrite & write
    );

    [[nodiscard]]
    std::expected<transaction::Lsn, WalError> append_commit(transaction::TransactionId transaction_id);

    [[nodiscard]]
    std::expected<void, WalError> flush_through(transaction::Lsn lsn);

    [[nodiscard]]
    std::expected<void, WalError> flush_all();

    [[nodiscard]]
    std::expected<WalScanResult, WalError> scan(
        bool truncate_incomplete_tail = true,
        const WalDecodeLimits & limits = {}
    );

    /**
     * @brief 发布下一代空 WAL，并清理旧的正式段
     */
    [[nodiscard]]
    std::expected<std::uint64_t, WalError> rotate(
        transaction::TransactionId checkpoint_transaction_id,
        const WalRotationHook & hook = {}
    );

    [[nodiscard]]
    WalManagerMetrics metrics() const noexcept;

    [[nodiscard]]
    const WalFileHeader & header() const noexcept;

private:
    [[nodiscard]]
    static std::filesystem::path segment_path(const std::filesystem::path & directory, std::uint64_t generation);

private:
    std::filesystem::path directory_;
    filesystem::FileSystem * filesystem_ {nullptr};
    WalStore active_;
    std::size_t retained_segments_ {1};
    WalDecodeLimits limits_;
    std::size_t record_count_ {0};
};

} // namespace litedb::core::wal

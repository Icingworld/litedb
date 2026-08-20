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

// 管理 WAL 分段的发现、当前段与轮换
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

    // 打开 WAL 目录；目录为空时创建第一代段，否则只打开最高代段。
    [[nodiscard]]
    static std::expected<WalManager, WalError> open(
        std::filesystem::path directory,
        filesystem::FileSystem & filesystem,
        WalDecodeLimits limits = {}
    );

    // 在追加 Begin/FileWrite/Commit 前验证整个事务是否仍可由相同预算恢复
    [[nodiscard]]
    std::expected<void, WalError> validate_transaction(std::span<const FileWrite> writes) const;

    // 追加事务开始标记，并更新当前段的记录计数。
    [[nodiscard]]
    std::expected<transaction::Lsn, WalError> append_begin(
        transaction::TransactionId transaction_id
    );

    // 追加文件写入记录，并更新当前段的记录计数。
    [[nodiscard]]
    std::expected<transaction::Lsn, WalError>
    append_write(transaction::TransactionId transaction_id, const FileWrite & write);

    // 追加事务提交标记，并更新当前段的记录计数。
    [[nodiscard]]
    std::expected<transaction::Lsn, WalError> append_commit(
        transaction::TransactionId transaction_id
    );

    // 刷盘并确认指定 LSN 之前的 WAL 数据。
    [[nodiscard]]
    std::expected<void, WalError> flush_through(transaction::Lsn lsn);

    // 刷盘当前段的数据和元数据。
    [[nodiscard]]
    std::expected<void, WalError> flush_all();

    // 扫描当前段，并按要求处理不完整尾部。
    [[nodiscard]]
    std::expected<WalScanResult, WalError>
    scan(bool truncate_incomplete_tail = true, const WalDecodeLimits & limits = {});

    // 轮换到下一代 WAL 段，并在发布后清理旧段。
    [[nodiscard]]
    std::expected<std::uint64_t, WalError>
    rotate(transaction::TransactionId checkpoint_transaction_id, const WalRotationHook & hook = {});

    // 返回当前 WAL 段及保留段数量等运行指标。
    [[nodiscard]]
    WalManagerMetrics metrics() const noexcept;

    // 返回当前活动段的已校验文件头。
    [[nodiscard]]
    const WalFileHeader & header() const noexcept;

    // 返回当前段是否已经进入只读恢复要求状态。
    [[nodiscard]]
    bool recovery_required() const noexcept;

private:
    // 按固定宽度生成 WAL 段文件名。
    [[nodiscard]]
    static std::filesystem::path
    segment_path(const std::filesystem::path & directory, std::uint64_t generation);

private:
    std::filesystem::path directory_;
    filesystem::FileSystem * filesystem_ {nullptr};
    WalStore active_;
    std::size_t retained_segments_ {1};
    WalDecodeLimits limits_;
    std::size_t record_count_ {0};
};

} // namespace litedb::core::wal

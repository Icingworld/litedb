#pragma once

#include <cstdint>
#include <expected>
#include <filesystem>
#include <optional>
#include <span>

#include "core/filesystem/file_handle.hpp"
#include "core/filesystem/filesystem.hpp"
#include "core/wal/wal_codec.hpp"

namespace litedb::core::wal
{

// 一个 WAL 段文件的持久化存储。
// WalStore 只负责单个已校验段的生命周期、追加、刷盘和扫描；段发现与轮换由
// WalManager 负责。create() 与 open() 有意保持严格区分，避免恢复流程意外创建文件。
class WalStore final
{
private:
    WalStore(
        std::filesystem::path path,
        filesystem::FileHandle file,
        WalFileHeader header,
        std::uint64_t size_bytes
    ) noexcept;

public:
    WalStore(const WalStore &) = delete;
    WalStore & operator=(const WalStore &) = delete;
    WalStore(WalStore &&) noexcept = default;
    WalStore & operator=(WalStore &&) noexcept = default;

    // 创建一个新的 WAL 段并持久化文件头。
    [[nodiscard]]
    static std::expected<WalStore, WalError>
    create(std::filesystem::path path, filesystem::FileSystem & filesystem, WalFileHeader header);

    // 仅打开并校验已存在的 WAL 段。
    [[nodiscard]]
    static std::expected<WalStore, WalError>
    open(std::filesystem::path path, filesystem::FileSystem & filesystem);

    // 追加事务开始记录。
    [[nodiscard]]
    std::expected<transaction::Lsn, WalError> append_begin(
        transaction::TransactionId transaction_id
    );

    // 追加文件写入记录。
    [[nodiscard]]
    std::expected<transaction::Lsn, WalError>
    append_write(transaction::TransactionId transaction_id, const FileWrite & write);

    // 追加事务提交记录。
    [[nodiscard]]
    std::expected<transaction::Lsn, WalError> append_commit(
        transaction::TransactionId transaction_id
    );

    // 刷盘并确认指定 LSN 已提交到文件系统。
    [[nodiscard]]
    std::expected<void, WalError> flush_through(transaction::Lsn lsn);

    // 刷盘文件数据及元数据。
    [[nodiscard]]
    std::expected<void, WalError> flush_all();

    // 扫描段内容。
    // truncate_incomplete_tail 指定是否截断不完整尾部；RecoveryRequired 时必须为 false。
    [[nodiscard]]
    std::expected<WalScanResult, WalError>
    scan(bool truncate_incomplete_tail = true, const WalDecodeLimits & limits = {});

    // 将段截断到指定的有效长度并同步。
    [[nodiscard]]
    std::expected<void, WalError> truncate_tail(std::uint64_t valid_size);

    // 返回该段文件路径。
    [[nodiscard]]
    const std::filesystem::path & path() const noexcept;

    // 返回最近一次成功确认的 LSN。
    [[nodiscard]]
    std::optional<transaction::Lsn> flushed_lsn() const noexcept;

    // 返回缓存的文件长度。
    [[nodiscard]]
    std::uint64_t size_bytes() const noexcept;

    // 返回已校验的文件头。
    [[nodiscard]]
    const WalFileHeader & header() const noexcept;

    // 返回段是否因无法确认回滚或持久性而需要恢复。
    [[nodiscard]]
    bool recovery_required() const noexcept;

private:
    // 追加一条记录，并在部分写入后尝试恢复旧尾部。
    [[nodiscard]]
    std::expected<transaction::Lsn, WalError> append(
        WalRecordType type,
        transaction::TransactionId transaction_id,
        std::span<const std::byte> payload
    );

    // 生成 RecoveryRequired 错误。
    [[nodiscard]]
    WalError recovery_error(WalOperation operation, std::optional<transaction::Lsn> lsn = {}) const;

private:
    std::filesystem::path path_;
    filesystem::FileHandle file_;
    WalFileHeader header_;
    std::optional<transaction::Lsn> flushed_lsn_;
    std::uint64_t size_bytes_ {0};
    bool recovery_required_ {false};
};

} // namespace litedb::core::wal

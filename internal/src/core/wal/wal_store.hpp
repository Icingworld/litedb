#pragma once

#include <cstdint>
#include <expected>
#include <filesystem>
#include <optional>
#include <span>

#include "core/filesystem/file_handle.hpp"
#include "core/filesystem/filesystem.hpp"
#include "core/wal/wal_error.hpp"
#include "core/wal/wal_types.hpp"
#include "core/transaction/transaction_id.hpp"

namespace litedb::core::wal
{

// WAL 文件的持久化存储
class WalStore
{
public:
    WalStore(const WalStore &) = delete;

    WalStore & operator=(const WalStore &) = delete;

    WalStore(WalStore &&) noexcept = default;

    WalStore & operator=(WalStore &&) noexcept = default;

private:
    WalStore(
        std::filesystem::path path,
        filesystem::FileHandle file,
        WalFileHeader header,
        std::uint64_t size_bytes
    ) noexcept;

public:
    // 创建一个新的 WAL 文件并持久化文件头
    [[nodiscard]]
    static std::expected<WalStore, WalError>
    create(std::filesystem::path path, filesystem::FileSystem & filesystem, WalFileHeader header);

    // 打开并校验已存在的 WAL 文件
    [[nodiscard]]
    static std::expected<WalStore, WalError>
    open(std::filesystem::path path, filesystem::FileSystem & filesystem);

    // 追加事务开始记录
    [[nodiscard]]
    std::expected<transaction::Lsn, WalError> append_begin(
        transaction::TransactionId transaction_id
    );

    // 追加文件写入记录
    [[nodiscard]]
    std::expected<transaction::Lsn, WalError>
    append_write(transaction::TransactionId transaction_id, const FileWrite & write);

    // 追加事务提交记录
    [[nodiscard]]
    std::expected<transaction::Lsn, WalError> append_commit(
        transaction::TransactionId transaction_id
    );

    // 刷盘并确认指定 LSN 已提交到文件系统
    [[nodiscard]]
    std::expected<void, WalError> flush_through(transaction::Lsn lsn);

    // 刷盘文件数据及元数据
    [[nodiscard]]
    std::expected<void, WalError> flush_all();

    // 扫描文件内容
    // truncate_incomplete_tail 指定是否截断不完整尾部；RecoveryRequired 时必须为 false
    [[nodiscard]]
    std::expected<WalScanResult, WalError>
    scan(bool truncate_incomplete_tail = true, const WalDecodeLimits & limits = {});

    // 将文件截断到指定的有效长度并同步
    [[nodiscard]]
    std::expected<void, WalError> truncate_tail(std::uint64_t valid_size);

    // 返回该文件文件路径
    [[nodiscard]]
    const std::filesystem::path & path() const noexcept;

    // 返回最近一次成功确认的 LSN
    [[nodiscard]]
    std::optional<transaction::Lsn> flushed_lsn() const noexcept;

    // 返回缓存的文件长度
    [[nodiscard]]
    std::uint64_t size_bytes() const noexcept;

    // 返回已校验的文件头
    [[nodiscard]]
    const WalFileHeader & header() const noexcept;

    // 返回文件是否因无法确认回滚或持久性而需要恢复
    [[nodiscard]]
    bool recovery_required() const noexcept;

private:
    // 追加一条记录，并在部分写入后尝试恢复旧尾部
    [[nodiscard]]
    std::expected<transaction::Lsn, WalError> append(
        WalRecordType type,
        transaction::TransactionId transaction_id,
        std::span<const std::byte> payload
    );

    // 生成 RecoveryRequired 错误
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

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

/**
 * @brief WAL 持久化存储
 */
class WalStore final
{
private:
    WalStore(std::filesystem::path path, filesystem::FileHandle file, std::uint64_t size_bytes) noexcept;

public:
    WalStore(const WalStore &) = delete;

    WalStore & operator=(const WalStore &) = delete;

    WalStore(WalStore &&) noexcept = default;

    WalStore & operator=(WalStore &&) noexcept = default;

public:
    /**
     * @brief 打开或创建 WAL 文件
     * @param path 文件路径
     * @param filesystem 文件系统
     * @return WAL 存储
     */
    [[nodiscard]]
    static std::expected<WalStore, WalError> open(
        std::filesystem::path path,
        filesystem::FileSystem & filesystem
    );

    /**
     * @brief 追加事务开始记录
     * @param transaction_id 事务 ID
     * @return 日志序列号
     */
    [[nodiscard]]
    std::expected<transaction::Lsn, WalError> append_begin(transaction::TransactionId transaction_id);

    /**
     * @brief 追加文件写入记录
     * @param transaction_id 事务 ID
     * @param write 文件写入
     * @return 日志序列号
     */
    [[nodiscard]]
    std::expected<transaction::Lsn, WalError> append_write(
        transaction::TransactionId transaction_id,
        const FileWrite & write
    );

    /**
     * @brief 追加事务提交记录
     * @param transaction_id 事务 ID
     * @return 日志序列号
     */
    [[nodiscard]]
    std::expected<transaction::Lsn, WalError> append_commit(transaction::TransactionId transaction_id);

    /**
     * @brief 刷盘至指定 LSN
     * @param lsn 日志序列号
     * @return 结果
     */
    [[nodiscard]]
    std::expected<void, WalError> flush_through(transaction::Lsn lsn);

    /**
     * @brief 扫描 WAL 记录
     * @param truncate_incomplete_tail 是否截断不完整尾部
     * @return 扫描结果
     */
    [[nodiscard]]
    std::expected<WalScanResult, WalError> scan(bool truncate_incomplete_tail = true);

    /**
     * @brief 截断 WAL 尾部
     * @param valid_size 有效大小
     * @return 结果
     */
    [[nodiscard]]
    std::expected<void, WalError> truncate_tail(std::uint64_t valid_size);

    /**
     * @brief 获取 WAL 文件路径
     * @return 文件路径
     */
    [[nodiscard]]
    const std::filesystem::path & path() const noexcept;

    /**
     * @brief 获取已刷盘的最大 LSN
     * @return 已刷盘 LSN
     */
    [[nodiscard]]
    std::optional<transaction::Lsn> flushed_lsn() const noexcept;

    [[nodiscard]]
    std::uint64_t size_bytes() const noexcept;

private:
    /**
     * @brief 追加 WAL 记录
     * @param type 记录类型
     * @param transaction_id 事务 ID
     * @param payload 负载数据
     * @return 日志序列号
     */
    [[nodiscard]]
    std::expected<transaction::Lsn, WalError> append(
        WalRecordType type,
        transaction::TransactionId transaction_id,
        std::span<const std::byte> payload
    );

private:
    std::filesystem::path path_;                        ///< 文件路径
    filesystem::FileHandle file_;                       ///< 文件句柄
    std::optional<transaction::Lsn> flushed_lsn_;       ///< 已刷盘 LSN
    std::uint64_t size_bytes_ {0};                      ///< 当前 WAL 大小
};

} // namespace litedb::core::wal

#pragma once

#include <cstddef>
#include <expected>
#include <filesystem>
#include <functional>

#include "core/filesystem/filesystem.hpp"
#include "core/wal/wal_store.hpp"

namespace litedb::core::wal
{

/**
 * @brief 恢复结果
 */
struct RecoveryResult
{
    transaction::TransactionId maximum_transaction_id {transaction::InvalidTransactionId}; ///< 最大事务 ID
    std::size_t committed_transactions {0};  ///< 已提交事务数
    std::size_t replayed_writes {0};         ///< 重放写入数
};

/**
 * @brief WAL 恢复管理器
 */
class RecoveryManager final
{
public:
    /**
     * @brief 从 WAL 恢复数据目录
     * @param data_directory 数据目录
     * @param filesystem 文件系统
     * @param wal WAL 存储
     * @param is_live_target 判断目标是否仍有效的回调，可为空
     * @return 恢复结果
     */
    [[nodiscard]]
    static std::expected<RecoveryResult, WalError> recover(
        const std::filesystem::path & data_directory,
        filesystem::FileSystem & filesystem,
        WalStore & wal,
        std::function<bool(const FileTarget &)> is_live_target = {}
    );
};

} // namespace litedb::core::wal

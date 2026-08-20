#pragma once

#include <cstddef>
#include <expected>
#include <filesystem>

#include "core/filesystem/filesystem.hpp"
#include "core/wal/wal_manager.hpp"

namespace litedb::core::wal
{

// 恢复结果
struct RecoveryResult
{
    transaction::TransactionId maximum_transaction_id {transaction::InvalidTransactionId}; // 最大事务 ID
    std::size_t committed_transactions {0};  // 已提交事务数
    std::size_t replayed_writes {0};         // 重放写入数
};

// WAL 恢复管理器
class RecoveryManager final
{
public:
    // 从 WAL 恢复数据目录
    [[nodiscard]]
    static std::expected<RecoveryResult, WalError> recover(
        const std::filesystem::path & data_directory,
        filesystem::FileSystem & filesystem,
        WalManager & wal,
        const WalDecodeLimits & limits = {}
    );
};

} // namespace litedb::core::wal

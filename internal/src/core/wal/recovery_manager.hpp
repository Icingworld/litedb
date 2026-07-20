#pragma once

#include <expected>
#include <filesystem>
#include <functional>

#include "core/filesystem/filesystem.hpp"
#include "core/wal/wal_store.hpp"

namespace litedb::core::wal
{

struct RecoveryResult
{
    transaction::TransactionId maximum_transaction_id {transaction::InvalidTransactionId};
    std::size_t committed_transactions {0};
    std::size_t replayed_writes {0};
};

class RecoveryManager final
{
public:
    [[nodiscard]] static std::expected<RecoveryResult, WalError> recover(
        const std::filesystem::path & data_directory,
        filesystem::FileSystem & filesystem,
        WalStore & wal,
        std::function<bool(const FileTarget &)> is_live_target = {}
    );
};

} // namespace litedb::core::wal

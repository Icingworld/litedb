#include "core/wal/recovery_manager.hpp"

#include <algorithm>
#include <unordered_map>
#include <utility>

#include "core/wal/file_write_batch.hpp"

namespace litedb::core::wal
{

std::expected<RecoveryResult, WalError> RecoveryManager::recover(
    const std::filesystem::path & data_directory,
    filesystem::FileSystem & filesystem,
    WalManager & wal,
    const WalDecodeLimits & limits
)
{
    auto scanned = wal.scan(true, limits);
    if (!scanned) {
        return std::unexpected(std::move(scanned.error()));
    }

    /**
     * @brief 事务日志状态
     */
    struct TransactionLogState
    {
        bool began {false};      ///< 是否已开始
        bool committed {false};  ///< 是否已提交
    };

    std::unordered_map<transaction::TransactionId, TransactionLogState> states;
    for (const auto & record : scanned->records) {
        auto & state = states[record.transaction_id];
        switch (record.type) {
        case WalRecordType::Begin:
            if (state.began || state.committed || !record.payload.empty()) {
                return std::unexpected(make_error(
                    WalErrorCode::CorruptedRecord,
                    "Invalid WAL transaction begin sequence",
                    {
                        .operation = WalOperation::Recover,
                        .transaction_id = record.transaction_id,
                        .lsn = record.lsn,
                    }
                ));
            }
            state.began = true;
            break;
        case WalRecordType::FileWrite:
            if (!state.began || state.committed) {
                return std::unexpected(make_error(
                    WalErrorCode::CorruptedRecord,
                    "WAL write is outside an active transaction",
                    {
                        .operation = WalOperation::Recover,
                        .transaction_id = record.transaction_id,
                        .lsn = record.lsn,
                    }
                ));
            }
            break;
        case WalRecordType::Commit:
            if (!state.began || state.committed || !record.payload.empty()) {
                return std::unexpected(make_error(
                    WalErrorCode::CorruptedRecord,
                    "Invalid WAL transaction commit sequence",
                    {
                        .operation = WalOperation::Recover,
                        .transaction_id = record.transaction_id,
                        .lsn = record.lsn,
                    }
                ));
            }
            state.committed = true;
            break;
        }
    }

    RecoveryResult result {
        .maximum_transaction_id = std::max(
            scanned->maximum_transaction_id,
            wal.header().checkpoint_transaction_id
        ),
    };
    for (const auto & [_, state] : states) {
        if (state.committed) {
            ++result.committed_transactions;
        }
    }

    std::unordered_map<transaction::TransactionId, FileWriteBatch> batches;
    for (auto & record : scanned->records) {
        if (!states[record.transaction_id].committed) {
            continue;
        }
        if (record.type == WalRecordType::FileWrite) {
            auto write = WalCodec::decode_file_write(std::move(record.payload));
            if (!write) {
                return std::unexpected(std::move(write.error()));
            }
            batches[record.transaction_id].add(std::move(*write));
            ++result.replayed_writes;
        } else if (record.type == WalRecordType::Commit) {
            auto batch = batches.find(record.transaction_id);
            if (batch != batches.end()) {
                auto applied = batch->second.apply(data_directory, filesystem, true);
                if (!applied) {
                    return std::unexpected(std::move(applied.error()));
                }
                batches.erase(batch);
            }
        }
    }
    return result;
}

} // namespace litedb::core::wal

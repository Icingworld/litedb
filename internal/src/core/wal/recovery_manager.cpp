#include "core/wal/recovery_manager.hpp"

#include <unordered_map>
#include <utility>

#include "core/wal/file_write_batch.hpp"

namespace litedb::core::wal
{

std::expected<RecoveryResult, WalError> RecoveryManager::recover(
    const std::filesystem::path & data_directory,
    filesystem::FileSystem & filesystem,
    WalStore & wal,
    std::function<bool(const FileTarget &)> is_live_target
)
{
    auto scanned = wal.scan(true);
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
                    "Invalid WAL transaction begin sequence"
                ));
            }
            state.began = true;
            break;
        case WalRecordType::FileWrite:
            if (!state.began || state.committed) {
                return std::unexpected(make_error(
                    WalErrorCode::CorruptedRecord,
                    "WAL write is outside an active transaction"
                ));
            }
            break;
        case WalRecordType::Commit:
            if (!state.began || state.committed || !record.payload.empty()) {
                return std::unexpected(make_error(
                    WalErrorCode::CorruptedRecord,
                    "Invalid WAL transaction commit sequence"
                ));
            }
            state.committed = true;
            break;
        }
    }

    RecoveryResult result {.maximum_transaction_id = scanned->maximum_transaction_id};
    for (const auto & [_, state] : states) {
        if (state.committed) {
            ++result.committed_transactions;
        }
    }

    FileWriteBatch batch;
    for (const auto & record : scanned->records) {
        if (record.type != WalRecordType::FileWrite || !states[record.transaction_id].committed) {
            continue;
        }

        auto write = WalCodec::decode_file_write(record.payload);
        if (!write) {
            return std::unexpected(std::move(write.error()));
        }
        if (is_live_target && !is_live_target(write->target)) {
            continue;
        }

        batch.add(std::move(*write));
        ++result.replayed_writes;
    }

    auto applied = batch.apply(data_directory, filesystem, true);
    if (!applied) {
        return std::unexpected(std::move(applied.error()));
    }
    return result;
}

} // namespace litedb::core::wal

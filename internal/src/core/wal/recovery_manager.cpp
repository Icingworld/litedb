#include "core/wal/recovery_manager.hpp"

#include <algorithm>
#include <unordered_map>
#include <utility>

#include "core/wal/file_write_batch.hpp"
#include "core/wal/wal_codec.hpp"

namespace litedb::core::wal
{

namespace
{

struct TransactionLogState
{
    bool began {false};
    bool committed {false};
};

using TransactionStates = std::unordered_map<transaction::TransactionId, TransactionLogState>;

// 只验证事务记录顺序，不触碰数据文件。
std::expected<TransactionStates, WalError> validate_transaction_sequences(
    const std::vector<WalRecord> & records
)
{
    TransactionStates states;
    for (const auto & record : records) {
        auto & state = states[record.transaction_id];
        switch (record.type) {
        case WalRecordType::Begin:
            if (state.began || state.committed || !record.payload.empty()) [[unlikely]] {
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
            if (!state.began || state.committed) [[unlikely]] {
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
            if (!state.began || state.committed || !record.payload.empty()) [[unlikely]] {
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
    return states;
}

// 仅重放已提交事务，忽略未提交事务的写集合。
std::expected<std::size_t, WalError> replay_committed_batches(
    const std::filesystem::path & data_directory,
    filesystem::FileSystem & filesystem,
    const std::vector<WalRecord> & records,
    const TransactionStates & states
)
{
    std::unordered_map<transaction::TransactionId, FileWriteBatch> batches;
    std::size_t replayed_writes {0};
    for (const auto & record : records) {
        const auto state = states.find(record.transaction_id);
        if (state == states.end() || !state->second.committed) {
            continue;
        }
        if (record.type == WalRecordType::FileWrite) {
            auto write = WalCodec::decode_file_write(record.payload);
            if (!write) [[unlikely]] {
                return std::unexpected(std::move(write.error()));
            }
            batches[record.transaction_id].add(std::move(*write));
            ++replayed_writes;
        } else if (record.type == WalRecordType::Commit) {
            auto batch = batches.find(record.transaction_id);
            if (batch != batches.end()) {
                auto applied = batch->second.apply(data_directory, filesystem, true);
                if (!applied) [[unlikely]] {
                    return std::unexpected(std::move(applied.error()));
                }
                batches.erase(batch);
            }
        }
    }
    return replayed_writes;
}

} // namespace

std::expected<RecoveryResult, WalError> RecoveryManager::recover(
    const std::filesystem::path & data_directory,
    filesystem::FileSystem & filesystem,
    WalManager & wal,
    const WalDecodeLimits & limits
)
{
    auto scanned = wal.scan(true, limits);
    if (!scanned) [[unlikely]] {
        return std::unexpected(std::move(scanned.error()));
    }
    auto states = validate_transaction_sequences(scanned->records);
    if (!states) [[unlikely]] {
        return std::unexpected(std::move(states.error()));
    }

    RecoveryResult result {
        .maximum_transaction_id =
            std::max(scanned->maximum_transaction_id, wal.header().checkpoint_transaction_id),
    };
    for (const auto & [_, state] : *states) {
        if (state.committed) {
            ++result.committed_transactions;
        }
    }
    auto replayed = replay_committed_batches(data_directory, filesystem, scanned->records, *states);
    if (!replayed) [[unlikely]] {
        return std::unexpected(std::move(replayed.error()));
    }
    result.replayed_writes = *replayed;
    return result;
}

} // namespace litedb::core::wal

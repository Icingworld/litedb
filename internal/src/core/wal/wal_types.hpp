#pragma once

#include <cstddef>
#include <cstdint>
#include <vector>

#include "core/transaction/transaction_id.hpp"

namespace litedb::core::wal
{

enum class FileKind : std::uint8_t
{
    CollectionStore = 1,
    ScalarIndex = 2,
    VectorIndex = 3,
};

struct FileTarget
{
    FileKind kind;
    std::uint64_t object_id;

    friend bool operator==(const FileTarget &, const FileTarget &) = default;
};

struct FileWrite
{
    FileTarget target;
    std::uint64_t offset;
    std::vector<std::byte> after_image;
};

enum class WalRecordType : std::uint8_t
{
    Begin = 1,
    FileWrite = 2,
    Commit = 3,
};

struct WalRecord
{
    WalRecordType type;
    transaction::Lsn lsn;
    transaction::TransactionId transaction_id;
    std::vector<std::byte> payload;
};

struct WalScanResult
{
    std::vector<WalRecord> records;
    std::uint64_t valid_size;
    bool truncated_tail {false};
    transaction::TransactionId maximum_transaction_id {transaction::InvalidTransactionId};
};

} // namespace litedb::core::wal

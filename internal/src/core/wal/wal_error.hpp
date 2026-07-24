#pragma once

#include <cstdint>
#include <filesystem>
#include <optional>
#include <string>
#include <utility>

#include "core/error/error.hpp"
#include "core/transaction/transaction_id.hpp"

namespace litedb::core::wal
{

enum class WalErrorCode : std::uint8_t
{
    FileSystemError = 0,
    InvalidFormat = 1,
    UnsupportedVersion = 2,
    CorruptedRecord = 3,
    InvalidRecord = 4,
    MissingTarget = 5,
    ResourceLimitExceeded = 6,
};

enum class WalOperation : std::uint8_t
{
    Encode,
    Decode,
    Open,
    Append,
    Flush,
    Scan,
    Truncate,
    Discover,
    Rotate,
    Apply,
    Recover,
};

struct WalErrorContext
{
    WalOperation operation {WalOperation::Decode};
    std::filesystem::path path;
    transaction::TransactionId transaction_id {transaction::InvalidTransactionId};
    std::optional<transaction::Lsn> lsn;
    std::optional<std::uint64_t> generation;
    std::optional<std::uint16_t> source_code;
};

using WalError = error::Error;

} // namespace litedb::core::wal

namespace litedb::core::error
{

template <>
struct ErrorTraits<wal::WalErrorCode>
{
    static constexpr ErrorCategory category = ErrorCategory::Wal;
};

} // namespace litedb::core::error

namespace litedb::core::wal
{

[[nodiscard]]
inline WalError make_error(
    WalErrorCode code,
    std::string message,
    WalErrorContext context = {}
)
{
    return WalError {code, message, std::move(context)};
}

} // namespace litedb::core::wal

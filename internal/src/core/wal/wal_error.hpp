#pragma once

#include <string>

namespace litedb::core::wal
{

enum class WalErrorCode
{
    FileSystemError,
    InvalidFormat,
    UnsupportedVersion,
    CorruptedRecord,
    InvalidRecord,
    MissingTarget,
};

struct WalError
{
    WalErrorCode code;
    std::string message;
};

} // namespace litedb::core::wal

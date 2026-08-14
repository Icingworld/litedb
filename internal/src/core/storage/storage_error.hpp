#pragma once

#include <cstdint>
#include <filesystem>
#include <optional>
#include <string>
#include <utility>

#include "core/common/ids.hpp"
#include "core/error/error.hpp"

namespace litedb::core::storage
{

// 存储错误码
enum class StorageErrorCode : std::uint8_t
{
    CollectionAlreadyExists = 0,
    CollectionNotFound = 1,
    CollectionStoreAlreadyExists = 2,
    CollectionStoreNotFound = 3,
    RecordNotFound = 4,
    ValueCountMismatch = 5,
    TypeMismatch = 6,
    NullConstraintViolation = 7,
    ValueTooLarge = 8,
    RecordTooLarge = 9,
    FileSystemFailure = 10,
    IoFailure = 11,
    UnexpectedEof = 12,
    InvalidFormat = 13,
    UnsupportedVersion = 14,
    CorruptedPage = 15,
    ChecksumMismatch = 16,
    ResourceLimitExceeded = 17,
    InvalidState = 18,
    DurabilityUnknown = 19,
    InvalidData = 20,
};

// 存储操作
enum class StorageOperation : std::uint8_t
{
    Create,
    Open,
    Load,
    Encode,
    Decode,
    ReadHeader,
    WriteHeader,
    ReadPage,
    WritePage,
    Insert,
    Update,
    Erase,
    Scan,
    Drop,
    Validate,
    Reload,
};

// 存储错误上下文
struct StorageErrorContext
{
    StorageOperation operation {StorageOperation::Load};
    std::filesystem::path path;
    common::CollectionId collection_id {0};
    common::RecordId record_id {0};
    std::optional<std::uint32_t> page_id;
    std::optional<std::uint16_t> slot_id;
    std::optional<std::uint16_t> source_code;
};

using StorageError = error::Error;

} // namespace litedb::core::storage

namespace litedb::core::error
{

template <>
struct ErrorTraits<storage::StorageErrorCode>
{
    static constexpr ErrorCategory category = ErrorCategory::Storage;
};

} // namespace litedb::core::error

namespace litedb::core::storage
{

[[nodiscard]]
inline StorageError make_storage_error(
    StorageErrorCode code,
    std::string message,
    StorageErrorContext context = {}
)
{
    return StorageError {code, message, std::move(context)};
}

} // namespace litedb::core::storage

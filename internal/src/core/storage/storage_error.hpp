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
    UnexpectedEof = 10,
    InvalidFormat = 11,
    UnsupportedVersion = 12,
    CorruptedPage = 13,
    ChecksumMismatch = 14,
    ResourceLimitExceeded = 15,
    InvalidState = 16,
    DurabilityUnknown = 17,
    InvalidData = 18,
};

// 存储操作
//
// operation 表示错误实际发生的最具体阶段：
// - 集合和记录的高层语义错误使用 Create/Open/Get/Insert/Update/Erase/Scan/Drop/Validate/Reload；
// - 编解码、文件头和数据页错误使用 Encode/Decode/ReadHeader/WriteHeader/ReadPage/WritePage；
// - Load 仅用于文件整体大小、页数、跨页唯一性等加载期全局不变量。
//
// 当高层操作进入更具体的阶段后，具体阶段覆盖高层操作。例如 Insert 中的数据页写入错误使用
// WritePage，记录编码错误使用 Encode。Filesystem/IO 错误直接穿透并保留其自身错误上下文。
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
    Get,
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

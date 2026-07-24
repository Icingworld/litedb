#pragma once

#include <cstdint>
#include <filesystem>
#include <optional>

#include "core/common/ids.hpp"
#include "core/error/error.hpp"

namespace litedb::core::index
{

enum class IndexErrorCode : std::uint8_t
{
    UnsupportedKeyType,
    InvalidKeyValue,
    KeyTypeMismatch,
    UnsupportedRangeScan,
    KeyNotFound,
    RecordNotFound,
    DuplicateEntry,
    DuplicateKey,
    IndexAlreadyExists,
    IndexNotFound,
    InvalidIndexColumn,
    StorageError,
    NotImplemented,
};

enum class IndexOperation : std::uint8_t
{
    ValidateKey,
    Create,
    Build,
    Open,
    Insert,
    Erase,
    Lookup,
    RangeScan,
    Drop,
    Restore,
    Reload,
    EncodePage,
    DecodePage,
    ReadPage,
    WritePage,
    Sync,
};

struct IndexErrorContext
{
    IndexOperation operation {IndexOperation::Lookup};
    common::IndexId index_id {0};
    std::optional<std::uint64_t> page_id;
    std::filesystem::path path;
    std::optional<std::uint16_t> source_code;
};

using IndexError = error::Error;

} // namespace litedb::core::index

namespace litedb::core::error
{

template <>
struct ErrorTraits<index::IndexErrorCode>
{
    static constexpr ErrorCategory category = ErrorCategory::Index;
};

} // namespace litedb::core::error

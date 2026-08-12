#pragma once

#include <cstdint>
#include <filesystem>
#include <optional>
#include <string>

#include "core/common/ids.hpp"
#include "core/error/error.hpp"

namespace litedb::core::catalog
{

enum class CatalogErrorCode : std::uint8_t
{
    InvalidArgument = 0,
    InvalidSnapshot = 1,
    DatabaseNotFound = 2,
    CollectionNotFound = 3,
    ColumnNotFound = 4,
    IndexNotFound = 5,
    VectorIndexNotFound = 6,
    DuplicateDatabase = 7,
    DuplicateCollection = 8,
    DuplicateColumn = 9,
    DuplicateIndex = 10,
    DuplicateVectorIndex = 11,
    FileSystemFailure = 12,
    IoFailure = 13,
    UnexpectedEof = 14,
    InvalidFormat = 15,
    UnsupportedVersion = 16,
    ValueTooLarge = 17,
    ResourceLimitExceeded = 18,
    ChecksumMismatch = 19,
    DurabilityUnknown = 20,
    InvalidState = 21,
};

enum class CatalogOperation : std::uint8_t
{
    Load,
    Decode,
    Encode,
    SaveTemporary,
    SyncTemporary,
    PublishFile,
    SyncDirectory,
    BuildCatalog,
    EditCatalog,
    PublishCatalog,
};

enum class CatalogEntityKind : std::uint8_t
{
    Database,
    Collection,
    Column,
    Index,
    VectorIndex,
};

struct CatalogErrorContext
{
    CatalogOperation operation {CatalogOperation::BuildCatalog};
    std::filesystem::path path;
    std::optional<CatalogEntityKind> entity_kind;
    common::CatalogEntryId entity_id {0};
    std::string entity_name;
    std::optional<std::uint16_t> source_code;
};

using CatalogError = error::Error;

[[nodiscard]]
CatalogError
make_error(CatalogErrorCode code, std::string message, CatalogErrorContext context = {});

} // namespace litedb::core::catalog

namespace litedb::core::error
{

template <>
struct ErrorTraits<catalog::CatalogErrorCode>
{
    static constexpr ErrorCategory category = ErrorCategory::Catalog;
};

} // namespace litedb::core::error

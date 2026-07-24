#pragma once

#include <cstdint>
#include <filesystem>
#include <optional>

#include "core/common/ids.hpp"
#include "core/error/error.hpp"

namespace litedb::core::vindex
{

/**
 * @brief 向量索引错误码
 */
enum class VectorIndexErrorCode : std::uint8_t
{
    UnsupportedMetric = 0,
    UnsupportedIndexKind = 1,
    InvalidDimension = 2,
    EmptyQuery = 3,
    RecordAlreadyExists = 4,
    RecordNotFound = 5,
    IndexAlreadyExists = 6,
    IndexNotFound = 7,
    InvalidMetadata = 8,
    IndexFileMissing = 9,
    CorruptedIndex = 10,
    StaleIndex = 11,
    FileSystemFailure = 12,
    StorageFailure = 13,
    InvalidVectorValue = 14,
    NumericOverflow = 15,
    InvalidMutation = 16,
    ResourceLimitExceeded = 17,
    DurabilityUnknown = 18,
    RecoveryRequired = 19,
    UnsupportedVersion = 20,
    ChecksumMismatch = 21,
    CorruptedGraph = 22,
};

enum class VectorIndexOperation : std::uint8_t
{
    ValidateKey,
    ComputeDistance,
    Create,
    Open,
    Build,
    Restore,
    Reload,
    Insert,
    Erase,
    Search,
    Commit,
    EncodeHeader,
    DecodeHeader,
    EncodeFrame,
    DecodeFrame,
    Read,
    Append,
    Truncate,
    Sync,
    Compact,
    Publish,
    Drop,
    Verify,
};

struct VectorIndexErrorContext
{
    VectorIndexOperation operation {VectorIndexOperation::Search};
    common::VIndexId index_id {0};
    common::CollectionId collection_id {0};
    common::ColumnId column_id {0};
    common::RecordId record_id {0};
    std::optional<std::uint64_t> node_id;
    std::optional<std::uint64_t> frame_sequence;
    std::filesystem::path path;
    std::optional<std::uint16_t> source_code;
};

using VectorIndexError = error::Error;

} // namespace litedb::core::vindex

namespace litedb::core::error
{

template <>
struct ErrorTraits<vindex::VectorIndexErrorCode>
{
    static constexpr ErrorCategory category = ErrorCategory::VectorIndex;
};

} // namespace litedb::core::error

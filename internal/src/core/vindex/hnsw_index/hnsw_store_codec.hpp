#pragma once

#include <array>
#include <cstddef>
#include <cstdint>
#include <expected>
#include <span>
#include <string>
#include <vector>

#include "core/common/ids.hpp"
#include "core/vindex/hnsw_index/hnsw_node.hpp"
#include "core/vindex/vector_index.hpp"

namespace litedb::core::vindex::hnsw_index
{

struct HnswStoreDescriptor
{
    common::VIndexId index_id {0};
    common::CollectionId collection_id {0};
    common::ColumnId column_id {0};
    std::size_t dimension {0};
    VectorDistanceMetric metric {VectorDistanceMetric::L2};
    std::size_t max_neighbors {16};
    std::size_t ef_construction {200};
    std::size_t ef_search_default {64};
    std::size_t random_seed {0};
};

enum class HnswStoreCodecErrorCode
{
    InvalidFormat,
    UnsupportedVersion,
    ValueOutOfRange,
    CorruptedData,
};

struct HnswStoreCodecError
{
    HnswStoreCodecErrorCode code;
    std::string message;
};

struct HnswCommitFrame
{
    HnswGraphMetadata metadata;
    std::vector<HnswNode> upserts;
};

class HnswStoreCodec final
{
public:
    static constexpr std::size_t HeaderSize = 4096;
    static constexpr std::size_t FramePrefixSize = 32;
    static constexpr std::uint64_t MaximumFrameSize = 64ULL << 20U;

    using HeaderBuffer = std::array<std::byte, HeaderSize>;

    [[nodiscard]]
    static std::expected<HeaderBuffer, HnswStoreCodecError> encode_header(
        const HnswStoreDescriptor & descriptor
    );

    [[nodiscard]]
    static std::expected<HnswStoreDescriptor, HnswStoreCodecError> decode_header(
        std::span<const std::byte> bytes
    );

    [[nodiscard]]
    static std::expected<std::vector<std::byte>, HnswStoreCodecError> encode_frame(
        const HnswCommitFrame & frame,
        std::size_t dimension
    );

    [[nodiscard]]
    static std::expected<std::uint64_t, HnswStoreCodecError> decode_frame_size(
        std::span<const std::byte> prefix
    );

    [[nodiscard]]
    static std::expected<HnswCommitFrame, HnswStoreCodecError> decode_frame(
        std::span<const std::byte> bytes,
        std::size_t dimension
    );
};

} // namespace litedb::core::vindex::hnsw_index

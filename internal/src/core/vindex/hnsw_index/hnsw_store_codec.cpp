#include "core/vindex/hnsw_index/hnsw_store_codec.hpp"

#include <bit>
#include <cstring>
#include <limits>
#include <type_traits>
#include <utility>

namespace litedb::core::vindex::hnsw_index
{

namespace
{

constexpr std::uint32_t HeaderMagic = 0x3157484c; // LHW1
constexpr std::uint32_t FrameMagic = 0x31435748;  // HWC1
constexpr std::uint16_t FormatVersion = 1;
constexpr std::size_t HeaderChecksumOffset = 80;
constexpr std::uint64_t MaximumNeighborCount = 1ULL << 20U;

using Error = VectorIndexError;
using ErrorCode = VectorIndexErrorCode;

[[nodiscard]]
Error error(
    ErrorCode code,
    std::string message,
    VectorIndexOperation operation = VectorIndexOperation::DecodeFrame
)
{
    return Error {code, message, VectorIndexErrorContext {.operation = operation}};
}

class Writer
{
public:
    explicit Writer(std::size_t reserve = 0) { bytes_.reserve(reserve); }

    template <typename T>
    requires std::is_integral_v<T>
    void number(T value)
    {
        using Unsigned = std::make_unsigned_t<T>;
        const auto bits = static_cast<Unsigned>(value);
        for (std::size_t index = 0; index < sizeof(T); ++index) {
            bytes_.push_back(static_cast<std::byte>(
                (bits >> (index * 8U)) & static_cast<Unsigned>(0xffU)
            ));
        }
    }

    void floating(double value) { number(std::bit_cast<std::uint64_t>(value)); }

    void zeros(std::size_t count) { bytes_.insert(bytes_.end(), count, std::byte {0}); }

    [[nodiscard]] std::vector<std::byte> take() && { return std::move(bytes_); }

private:
    std::vector<std::byte> bytes_;
};

class Reader
{
public:
    explicit Reader(std::span<const std::byte> bytes) noexcept : bytes_(bytes) {}

    template <typename T>
    requires std::is_integral_v<T>
    [[nodiscard]] std::optional<T> number() noexcept
    {
        if (remaining() < sizeof(T)) {
            return std::nullopt;
        }
        using Unsigned = std::make_unsigned_t<T>;
        Unsigned value {0};
        for (std::size_t index = 0; index < sizeof(T); ++index) {
            value |= static_cast<Unsigned>(std::to_integer<unsigned int>(bytes_[position_ + index]))
                << (index * 8U);
        }
        position_ += sizeof(T);
        return static_cast<T>(value);
    }

    [[nodiscard]] std::optional<double> floating() noexcept
    {
        auto bits = number<std::uint64_t>();
        return bits.has_value() ? std::optional<double> {std::bit_cast<double>(*bits)} : std::nullopt;
    }

    [[nodiscard]] bool skip(std::size_t count) noexcept
    {
        if (remaining() < count) {
            return false;
        }
        position_ += count;
        return true;
    }

    [[nodiscard]] std::size_t remaining() const noexcept { return bytes_.size() - position_; }

private:
    std::span<const std::byte> bytes_;
    std::size_t position_ {0};
};

template <typename T>
requires std::is_integral_v<T>
void write_at(std::span<std::byte> bytes, std::size_t offset, T value) noexcept
{
    using Unsigned = std::make_unsigned_t<T>;
    const auto bits = static_cast<Unsigned>(value);
    for (std::size_t index = 0; index < sizeof(T); ++index) {
        bytes[offset + index] = static_cast<std::byte>(
            (bits >> (index * 8U)) & static_cast<Unsigned>(0xffU)
        );
    }
}

[[nodiscard]]
std::uint32_t crc32(std::span<const std::byte> bytes) noexcept
{
    std::uint32_t crc = 0xffffffffU;
    for (const auto byte : bytes) {
        crc ^= std::to_integer<std::uint8_t>(byte);
        for (int bit = 0; bit < 8; ++bit) {
            const auto mask = static_cast<std::uint32_t>(-(static_cast<std::int32_t>(crc & 1U)));
            crc = (crc >> 1U) ^ (0xedb88320U & mask);
        }
    }
    return ~crc;
}

[[nodiscard]]
std::optional<std::uint8_t> encode_metric(VectorDistanceMetric metric) noexcept
{
    switch (metric) {
    case VectorDistanceMetric::L2: return 1;
    case VectorDistanceMetric::InnerProduct: return 2;
    case VectorDistanceMetric::Cosine: return 3;
    }
    return std::nullopt;
}

[[nodiscard]]
std::optional<VectorDistanceMetric> decode_metric(std::uint8_t metric) noexcept
{
    switch (metric) {
    case 1: return VectorDistanceMetric::L2;
    case 2: return VectorDistanceMetric::InnerProduct;
    case 3: return VectorDistanceMetric::Cosine;
    default: return std::nullopt;
    }
}

[[nodiscard]]
bool fits_size_t(std::uint64_t value) noexcept
{
    return value <= std::numeric_limits<std::size_t>::max();
}

[[nodiscard]]
std::expected<HnswStoreDescriptor, Error> decode_descriptor(Reader & reader)
{
    auto index_id = reader.number<common::VIndexId>();
    auto collection_id = reader.number<common::CollectionId>();
    auto column_id = reader.number<common::ColumnId>();
    auto dimension = reader.number<std::uint64_t>();
    auto metric_value = reader.number<std::uint8_t>();
    if (!reader.skip(7)) {
        return std::unexpected(error(ErrorCode::CorruptedIndex, "Truncated HNSW store header"));
    }
    auto max_neighbors = reader.number<std::uint64_t>();
    auto ef_construction = reader.number<std::uint64_t>();
    auto ef_search_default = reader.number<std::uint64_t>();
    auto random_seed = reader.number<std::uint64_t>();
    if (!index_id || !collection_id || !column_id || !dimension || !metric_value ||
        !max_neighbors || !ef_construction || !ef_search_default || !random_seed) {
        return std::unexpected(error(ErrorCode::CorruptedIndex, "Truncated HNSW store descriptor"));
    }
    const auto metric = decode_metric(*metric_value);
    if (!metric || !fits_size_t(*dimension) || !fits_size_t(*max_neighbors) ||
        !fits_size_t(*ef_construction) || !fits_size_t(*ef_search_default) ||
        !fits_size_t(*random_seed) || *max_neighbors > MaximumNeighborCount) {
        return std::unexpected(error(ErrorCode::ResourceLimitExceeded, "Invalid HNSW store descriptor value"));
    }
    return HnswStoreDescriptor {
        .index_id = *index_id,
        .collection_id = *collection_id,
        .column_id = *column_id,
        .dimension = static_cast<std::size_t>(*dimension),
        .metric = *metric,
        .max_neighbors = static_cast<std::size_t>(*max_neighbors),
        .ef_construction = static_cast<std::size_t>(*ef_construction),
        .ef_search_default = static_cast<std::size_t>(*ef_search_default),
        .random_seed = static_cast<std::size_t>(*random_seed),
    };
}

} // namespace

std::expected<HnswStoreCodec::HeaderBuffer, VectorIndexError> HnswStoreCodec::encode_header(
    const HnswStoreDescriptor & descriptor
)
{
    const auto metric = encode_metric(descriptor.metric);
    if (!metric || descriptor.index_id == 0 || descriptor.collection_id == 0 ||
        descriptor.column_id == 0 || descriptor.dimension == 0 || descriptor.max_neighbors == 0 ||
        descriptor.max_neighbors > MaximumNeighborCount ||
        descriptor.ef_construction < descriptor.max_neighbors || descriptor.ef_search_default == 0) {
        return std::unexpected(error(ErrorCode::CorruptedIndex, "Invalid HNSW store descriptor"));
    }

    Writer writer(HeaderSize);
    writer.number(HeaderMagic);
    writer.number(FormatVersion);
    writer.number(static_cast<std::uint16_t>(HeaderSize));
    writer.number(descriptor.index_id);
    writer.number(descriptor.collection_id);
    writer.number(descriptor.column_id);
    writer.number(static_cast<std::uint64_t>(descriptor.dimension));
    writer.number(*metric);
    writer.zeros(7);
    writer.number(static_cast<std::uint64_t>(descriptor.max_neighbors));
    writer.number(static_cast<std::uint64_t>(descriptor.ef_construction));
    writer.number(static_cast<std::uint64_t>(descriptor.ef_search_default));
    writer.number(static_cast<std::uint64_t>(descriptor.random_seed));
    writer.number(std::uint32_t {0});
    writer.zeros(HeaderSize - 84);
    auto encoded = std::move(writer).take();
    write_at<std::uint32_t>(encoded, HeaderChecksumOffset, crc32(encoded));

    HeaderBuffer result {};
    std::memcpy(result.data(), encoded.data(), encoded.size());
    return result;
}

std::expected<HnswStoreDescriptor, VectorIndexError> HnswStoreCodec::decode_header(
    std::span<const std::byte> bytes
)
{
    if (bytes.size() != HeaderSize) {
        return std::unexpected(error(ErrorCode::CorruptedIndex, "Invalid HNSW store header size"));
    }
    std::vector<std::byte> checked(bytes.begin(), bytes.end());
    Reader checksum_reader(bytes.subspan(HeaderChecksumOffset, sizeof(std::uint32_t)));
    const auto stored_checksum = checksum_reader.number<std::uint32_t>();
    write_at<std::uint32_t>(checked, HeaderChecksumOffset, 0);
    if (!stored_checksum || *stored_checksum != crc32(checked)) {
        return std::unexpected(error(ErrorCode::ChecksumMismatch, "HNSW store header checksum mismatch"));
    }

    Reader reader(bytes);
    const auto magic = reader.number<std::uint32_t>();
    const auto version = reader.number<std::uint16_t>();
    const auto header_size = reader.number<std::uint16_t>();
    if (!magic || *magic != HeaderMagic || !header_size || *header_size != HeaderSize) {
        return std::unexpected(error(ErrorCode::CorruptedIndex, "Invalid HNSW store header"));
    }
    if (!version || *version != FormatVersion) {
        return std::unexpected(error(ErrorCode::UnsupportedVersion, "Unsupported HNSW store version"));
    }
    return decode_descriptor(reader);
}

std::expected<std::vector<std::byte>, VectorIndexError> HnswStoreCodec::encode_frame(
    const HnswCommitFrame & frame,
    std::size_t dimension
)
{
    if (dimension == 0 || frame.metadata.frame_sequence == 0 || frame.metadata.next_node_id == 0) {
        return std::unexpected(error(ErrorCode::InvalidMutation, "Invalid HNSW commit metadata"));
    }
    Writer payload;
    payload.number(frame.metadata.next_node_id);
    payload.number(frame.metadata.entry_point);
    payload.number(static_cast<std::uint64_t>(frame.metadata.max_level));
    payload.number(static_cast<std::uint64_t>(frame.metadata.active_count));
    payload.number(static_cast<std::uint64_t>(frame.upserts.size()));
    for (const auto & node : frame.upserts) {
        if (node.node_id == InvalidHnswNodeId || node.record_id == 0 || node.vector.size() != dimension ||
            node.neighbors.size() != node.level + 1) {
            return std::unexpected(error(ErrorCode::InvalidMutation, "Invalid HNSW node in commit"));
        }
        payload.number(node.node_id);
        payload.number(node.record_id);
        payload.number(static_cast<std::uint64_t>(node.level));
        payload.number(static_cast<std::uint8_t>(node.deleted ? 1 : 0));
        payload.zeros(7);
        for (const auto value : node.vector) {
            payload.floating(value);
        }
        payload.number(static_cast<std::uint64_t>(node.neighbors.size()));
        for (const auto & layer : node.neighbors) {
            payload.number(static_cast<std::uint64_t>(layer.size()));
            for (const auto neighbor : layer) {
                payload.number(neighbor);
            }
        }
    }
    auto body = std::move(payload).take();
    if (body.size() > MaximumFrameSize - FramePrefixSize) {
        return std::unexpected(error(ErrorCode::ResourceLimitExceeded, "HNSW commit frame is too large"));
    }

    Writer writer(FramePrefixSize + body.size());
    writer.number(FrameMagic);
    writer.number(FormatVersion);
    writer.number(std::uint16_t {0});
    writer.number(static_cast<std::uint64_t>(FramePrefixSize + body.size()));
    writer.number(frame.metadata.frame_sequence);
    writer.number(crc32(body));
    writer.number(std::uint32_t {0});
    auto prefix = std::move(writer).take();
    prefix.insert(prefix.end(), body.begin(), body.end());
    return prefix;
}

std::expected<std::uint64_t, VectorIndexError> HnswStoreCodec::decode_frame_size(
    std::span<const std::byte> prefix
)
{
    if (prefix.size() != FramePrefixSize) {
        return std::unexpected(error(ErrorCode::CorruptedIndex, "Invalid HNSW frame prefix size"));
    }
    Reader reader(prefix);
    const auto magic = reader.number<std::uint32_t>();
    const auto version = reader.number<std::uint16_t>();
    const auto reserved = reader.number<std::uint16_t>();
    const auto size = reader.number<std::uint64_t>();
    if (!magic || *magic != FrameMagic || !reserved || *reserved != 0 || !size ||
        *size < FramePrefixSize || *size > MaximumFrameSize) {
        return std::unexpected(error(ErrorCode::CorruptedIndex, "Invalid HNSW commit frame prefix"));
    }
    if (!version || *version != FormatVersion) {
        return std::unexpected(error(ErrorCode::UnsupportedVersion, "Unsupported HNSW commit frame version"));
    }
    return *size;
}

std::expected<HnswCommitFrame, VectorIndexError> HnswStoreCodec::decode_frame(
    std::span<const std::byte> bytes,
    std::size_t dimension
)
{
    if (bytes.size() < FramePrefixSize) {
        return std::unexpected(error(ErrorCode::CorruptedIndex, "Truncated HNSW commit frame"));
    }
    auto frame_size = decode_frame_size(bytes.first(FramePrefixSize));
    if (!frame_size || *frame_size != bytes.size()) {
        return std::unexpected(frame_size ? error(ErrorCode::CorruptedIndex, "HNSW frame size mismatch") : std::move(frame_size.error()));
    }
    Reader prefix(bytes.first(FramePrefixSize));
    (void) prefix.skip(16);
    const auto frame_sequence = prefix.number<std::uint64_t>();
    const auto stored_checksum = prefix.number<std::uint32_t>();
    const auto reserved = prefix.number<std::uint32_t>();
    const auto payload = bytes.subspan(FramePrefixSize);
    if (!frame_sequence || !stored_checksum || !reserved || *reserved != 0 || *stored_checksum != crc32(payload)) {
        return std::unexpected(error(ErrorCode::ChecksumMismatch, "HNSW commit frame checksum mismatch"));
    }

    Reader reader(payload);
    auto next_node_id = reader.number<HnswNodeId>();
    auto entry_point = reader.number<HnswNodeId>();
    auto max_level = reader.number<std::uint64_t>();
    auto active_count = reader.number<std::uint64_t>();
    auto upsert_count = reader.number<std::uint64_t>();
    if (!next_node_id || !entry_point || !max_level || !active_count || !upsert_count ||
        !fits_size_t(*max_level) || !fits_size_t(*active_count) || !fits_size_t(*upsert_count)) {
        return std::unexpected(error(ErrorCode::ResourceLimitExceeded, "Invalid HNSW commit metadata"));
    }
    if (dimension > reader.remaining() / sizeof(double)) {
        return std::unexpected(error(ErrorCode::CorruptedGraph, "HNSW vector dimension exceeds the commit payload"));
    }
    const auto minimum_node_size = 48U + dimension * sizeof(double);
    if (minimum_node_size == 0 || *upsert_count > reader.remaining() / minimum_node_size) {
        return std::unexpected(error(ErrorCode::CorruptedGraph, "HNSW node count exceeds the commit payload"));
    }

    HnswCommitFrame frame;
    frame.metadata = {
        .next_node_id = *next_node_id,
        .entry_point = *entry_point,
        .max_level = static_cast<std::size_t>(*max_level),
        .active_count = static_cast<std::size_t>(*active_count),
        .frame_sequence = *frame_sequence,
    };
    frame.upserts.reserve(static_cast<std::size_t>(*upsert_count));
    for (std::size_t index = 0; index < *upsert_count; ++index) {
        auto node_id = reader.number<HnswNodeId>();
        auto record_id = reader.number<common::RecordId>();
        auto level = reader.number<std::uint64_t>();
        auto deleted = reader.number<std::uint8_t>();
        if (!reader.skip(7) || !node_id || !record_id || !level || !deleted || *deleted > 1 ||
            !fits_size_t(*level) || *level > 63) {
            return std::unexpected(error(ErrorCode::CorruptedGraph, "Invalid HNSW node header"));
        }
        HnswNode node {
            .node_id = *node_id,
            .record_id = *record_id,
            .level = static_cast<std::size_t>(*level),
            .deleted = *deleted != 0,
        };
        node.vector.reserve(dimension);
        for (std::size_t value_index = 0; value_index < dimension; ++value_index) {
            auto value = reader.floating();
            if (!value) {
                return std::unexpected(error(ErrorCode::CorruptedGraph, "Truncated HNSW node vector"));
            }
            node.vector.push_back(*value);
        }
        auto layer_count = reader.number<std::uint64_t>();
        if (!layer_count || *layer_count != node.level + 1) {
            return std::unexpected(error(ErrorCode::CorruptedGraph, "Invalid HNSW node layer count"));
        }
        node.neighbors.resize(static_cast<std::size_t>(*layer_count));
        for (auto & layer : node.neighbors) {
            auto neighbor_count = reader.number<std::uint64_t>();
            if (!neighbor_count || !fits_size_t(*neighbor_count) || *neighbor_count > (1ULL << 20U) ||
                *neighbor_count > reader.remaining() / sizeof(HnswNodeId)) {
                return std::unexpected(error(ErrorCode::CorruptedGraph, "Invalid HNSW neighbor count"));
            }
            layer.reserve(static_cast<std::size_t>(*neighbor_count));
            for (std::size_t neighbor_index = 0; neighbor_index < *neighbor_count; ++neighbor_index) {
                auto neighbor = reader.number<HnswNodeId>();
                if (!neighbor) {
                    return std::unexpected(error(ErrorCode::CorruptedGraph, "Truncated HNSW neighbor list"));
                }
                layer.push_back(*neighbor);
            }
        }
        frame.upserts.push_back(std::move(node));
    }
    if (reader.remaining() != 0) {
        return std::unexpected(error(ErrorCode::CorruptedIndex, "Unexpected bytes after HNSW commit frame"));
    }
    return frame;
}

} // namespace litedb::core::vindex::hnsw_index

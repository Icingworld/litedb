#include "core/vindex/hnsw_index/hnsw_store.hpp"

#include <algorithm>
#include <cmath>
#include <limits>
#include <unordered_set>
#include <utility>

#include "core/filesystem/backend/filesystem_backend.hpp"

namespace litedb::core::vindex::hnsw_index
{

namespace
{

using Error = HnswStoreError;
using ErrorCode = HnswStoreErrorCode;

[[nodiscard]]
Error error(ErrorCode code, std::string message)
{
    return Error {code, std::move(message)};
}

[[nodiscard]]
Error filesystem_error(filesystem::FileSystemError value)
{
    return error(ErrorCode::FileSystemError, std::move(value.message));
}

[[nodiscard]]
Error codec_error(HnswStoreCodecError value)
{
    switch (value.code) {
    case HnswStoreCodecErrorCode::UnsupportedVersion:
        return error(ErrorCode::UnsupportedVersion, std::move(value.message));
    case HnswStoreCodecErrorCode::InvalidFormat:
    case HnswStoreCodecErrorCode::ValueOutOfRange:
        return error(ErrorCode::InvalidFormat, std::move(value.message));
    case HnswStoreCodecErrorCode::CorruptedData:
        return error(ErrorCode::CorruptedGraph, std::move(value.message));
    }
    return error(ErrorCode::InvalidFormat, "Unknown HNSW codec error");
}

} // namespace

HnswStore::HnswStore(
    std::filesystem::path path,
    HnswStoreDescriptor descriptor,
    filesystem::FileHandle file
) noexcept
    : path_(std::move(path))
    , descriptor_(descriptor)
    , file_(std::move(file))
{
}

std::expected<HnswStore, HnswStoreError> HnswStore::create(
    std::filesystem::path path,
    HnswStoreDescriptor descriptor,
    filesystem::FileSystem & filesystem
)
{
    auto header = HnswStoreCodec::encode_header(descriptor);
    if (!header) {
        return std::unexpected(codec_error(std::move(header.error())));
    }
    const auto parent = path.parent_path();
    if (!parent.empty()) {
        auto created = filesystem.create_dir_all(parent);
        if (!created) {
            return std::unexpected(filesystem_error(std::move(created.error())));
        }
    }
    auto opened = filesystem.open(path, {
        .access = filesystem::FileAccess::ReadWrite,
        .create_mode = filesystem::FileCreateMode::CreateNew,
    });
    if (!opened) {
        return std::unexpected(filesystem_error(std::move(opened.error())));
    }
    auto written = opened->write_at(0, *header);
    if (!written) {
        return std::unexpected(filesystem_error(std::move(written.error())));
    }
    auto synced = opened->sync_all();
    if (!synced) {
        return std::unexpected(filesystem_error(std::move(synced.error())));
    }
    return HnswStore {std::move(path), descriptor, std::move(*opened)};
}

std::expected<HnswStore, HnswStoreError> HnswStore::open(
    std::filesystem::path path,
    const HnswStoreDescriptor & expected,
    filesystem::FileSystem & filesystem
)
{
    auto opened = filesystem.open(path, {
        .access = filesystem::FileAccess::ReadWrite,
        .create_mode = filesystem::FileCreateMode::OpenExisting,
    });
    if (!opened) {
        return std::unexpected(filesystem_error(std::move(opened.error())));
    }
    HnswStore store {std::move(path), expected, std::move(*opened)};
    auto loaded = store.load();
    if (!loaded) {
        return std::unexpected(std::move(loaded.error()));
    }
    return store;
}

const std::filesystem::path & HnswStore::path() const noexcept
{
    return path_;
}

const HnswStoreDescriptor & HnswStore::descriptor() const noexcept
{
    return descriptor_;
}

const HnswGraphMetadata & HnswStore::metadata() const noexcept
{
    return metadata_;
}

const HnswStore::NodeMap & HnswStore::nodes() const noexcept
{
    return nodes_;
}

const HnswNode * HnswStore::find_node(HnswNodeId node_id) const noexcept
{
    const auto found = nodes_.find(node_id);
    return found == nodes_.end() ? nullptr : &found->second;
}

std::optional<HnswNodeId> HnswStore::find_active_node(common::RecordId record_id) const noexcept
{
    const auto found = active_nodes_.find(record_id);
    return found == active_nodes_.end() ? std::nullopt : std::optional<HnswNodeId> {found->second};
}

std::expected<void, HnswStoreError> HnswStore::commit(
    HnswGraphMetadata metadata,
    std::vector<HnswNode> upserts
)
{
    if (upserts.empty()) {
        return std::unexpected(error(ErrorCode::InvalidMutation, "HNSW commit has no node mutations"));
    }
    if (metadata_.frame_sequence == std::numeric_limits<std::uint64_t>::max()) {
        return std::unexpected(error(ErrorCode::InvalidMutation, "HNSW frame sequence is exhausted"));
    }
    metadata.frame_sequence = metadata_.frame_sequence + 1;

    NodeMap next_nodes = nodes_;
    std::unordered_set<HnswNodeId> mutated;
    for (const auto & node : upserts) {
        if (!mutated.insert(node.node_id).second) {
            return std::unexpected(error(ErrorCode::InvalidMutation, "HNSW commit mutates a node more than once"));
        }
        next_nodes.insert_or_assign(node.node_id, node);
    }
    auto valid = validate_graph(next_nodes, metadata);
    if (!valid) {
        return valid;
    }

    auto encoded = HnswStoreCodec::encode_frame(HnswCommitFrame {
        .metadata = metadata,
        .upserts = std::move(upserts),
    }, descriptor_.dimension);
    if (!encoded) {
        return std::unexpected(codec_error(std::move(encoded.error())));
    }
    auto previous_size = file_.size();
    if (!previous_size) {
        return std::unexpected(filesystem_error(std::move(previous_size.error())));
    }
    auto appended = file_.append(*encoded);
    if (!appended) {
        (void) file_.truncate(*previous_size);
        (void) file_.sync_data();
        return std::unexpected(filesystem_error(std::move(appended.error())));
    }
    auto synced = file_.sync_data();
    if (!synced) {
        (void) file_.truncate(*previous_size);
        (void) file_.sync_data();
        return std::unexpected(filesystem_error(std::move(synced.error())));
    }

    nodes_.swap(next_nodes);
    metadata_ = metadata;
    active_nodes_.clear();
    active_nodes_.reserve(metadata_.active_count);
    for (const auto & [node_id, node] : nodes_) {
        if (!node.deleted) {
            active_nodes_.emplace(node.record_id, node_id);
        }
    }
    return {};
}

std::expected<void, HnswStoreError> HnswStore::load()
{
    auto file_size = file_.size();
    if (!file_size) {
        return std::unexpected(filesystem_error(std::move(file_size.error())));
    }
    if (*file_size < HnswStoreCodec::HeaderSize) {
        return std::unexpected(error(ErrorCode::InvalidFormat, "HNSW store file is smaller than its header"));
    }
    HnswStoreCodec::HeaderBuffer header {};
    auto header_read = file_.read_at(0, header);
    if (!header_read) {
        return std::unexpected(filesystem_error(std::move(header_read.error())));
    }
    if (*header_read != header.size()) {
        return std::unexpected(error(ErrorCode::InvalidFormat, "HNSW store header is truncated"));
    }
    auto decoded_header = HnswStoreCodec::decode_header(header);
    if (!decoded_header) {
        return std::unexpected(codec_error(std::move(decoded_header.error())));
    }
    if (!same_descriptor(*decoded_header, descriptor_)) {
        return std::unexpected(error(ErrorCode::InvalidFormat, "HNSW store descriptor does not match catalog metadata"));
    }

    std::uint64_t offset = HnswStoreCodec::HeaderSize;
    while (offset < *file_size) {
        const auto remaining = *file_size - offset;
        if (remaining < HnswStoreCodec::FramePrefixSize) {
            auto truncated = file_.truncate(offset);
            if (!truncated) {
                return std::unexpected(filesystem_error(std::move(truncated.error())));
            }
            auto synced = file_.sync_data();
            if (!synced) {
                return std::unexpected(filesystem_error(std::move(synced.error())));
            }
            break;
        }
        std::array<std::byte, HnswStoreCodec::FramePrefixSize> prefix {};
        auto prefix_read = file_.read_at(offset, prefix);
        if (!prefix_read) {
            return std::unexpected(filesystem_error(std::move(prefix_read.error())));
        }
        if (*prefix_read != prefix.size()) {
            return std::unexpected(error(ErrorCode::InvalidFormat, "HNSW commit prefix is truncated"));
        }
        auto frame_size = HnswStoreCodec::decode_frame_size(prefix);
        if (!frame_size) {
            return std::unexpected(codec_error(std::move(frame_size.error())));
        }
        if (*frame_size > remaining) {
            auto truncated = file_.truncate(offset);
            if (!truncated) {
                return std::unexpected(filesystem_error(std::move(truncated.error())));
            }
            auto synced = file_.sync_data();
            if (!synced) {
                return std::unexpected(filesystem_error(std::move(synced.error())));
            }
            break;
        }
        std::vector<std::byte> bytes(static_cast<std::size_t>(*frame_size));
        auto frame_read = file_.read_at(offset, bytes);
        if (!frame_read) {
            return std::unexpected(filesystem_error(std::move(frame_read.error())));
        }
        if (*frame_read != bytes.size()) {
            return std::unexpected(error(ErrorCode::InvalidFormat, "HNSW commit frame is truncated"));
        }
        auto frame = HnswStoreCodec::decode_frame(bytes, descriptor_.dimension);
        if (!frame) {
            return std::unexpected(codec_error(std::move(frame.error())));
        }
        if (frame->metadata.frame_sequence != metadata_.frame_sequence + 1) {
            return std::unexpected(error(ErrorCode::CorruptedGraph, "HNSW transaction ids are not contiguous"));
        }
        std::unordered_set<HnswNodeId> mutated;
        for (auto & node : frame->upserts) {
            if (!mutated.insert(node.node_id).second) {
                return std::unexpected(error(ErrorCode::CorruptedGraph, "HNSW frame contains duplicate node mutations"));
            }
            nodes_.insert_or_assign(node.node_id, std::move(node));
        }
        metadata_ = frame->metadata;
        offset += *frame_size;
    }

    auto valid = validate_graph(nodes_, metadata_);
    if (!valid) {
        return valid;
    }
    active_nodes_.clear();
    active_nodes_.reserve(metadata_.active_count);
    for (const auto & [node_id, node] : nodes_) {
        if (!node.deleted) {
            active_nodes_.emplace(node.record_id, node_id);
        }
    }
    return {};
}

std::expected<void, HnswStoreError> HnswStore::validate_graph(
    const NodeMap & nodes,
    const HnswGraphMetadata & metadata
) const
{
    if (metadata.next_node_id == InvalidHnswNodeId) {
        return std::unexpected(error(ErrorCode::CorruptedGraph, "HNSW next node id is invalid"));
    }
    if (nodes.empty()) {
        if (metadata.entry_point != InvalidHnswNodeId || metadata.max_level != 0 || metadata.active_count != 0) {
            return std::unexpected(error(ErrorCode::CorruptedGraph, "Empty HNSW graph has non-empty metadata"));
        }
        return {};
    }
    const auto entry = nodes.find(metadata.entry_point);
    if (metadata.entry_point == InvalidHnswNodeId || entry == nodes.end() || entry->second.level != metadata.max_level) {
        return std::unexpected(error(ErrorCode::CorruptedGraph, "HNSW entry point metadata is invalid"));
    }

    std::unordered_set<common::RecordId> active_records;
    std::size_t active_count = 0;
    HnswNodeId maximum_node_id = 0;
    for (const auto & [node_id, node] : nodes) {
        maximum_node_id = std::max(maximum_node_id, node_id);
        if (node.node_id != node_id || node_id == InvalidHnswNodeId || node.record_id == 0 ||
            node.vector.size() != descriptor_.dimension || node.level > 63 ||
            node.neighbors.size() != node.level + 1) {
            return std::unexpected(error(ErrorCode::CorruptedGraph, "HNSW node structure is invalid"));
        }
        if (std::ranges::any_of(node.vector, [](double value) { return !std::isfinite(value); })) {
            return std::unexpected(error(ErrorCode::CorruptedGraph, "HNSW node contains a non-finite vector value"));
        }
        if (!node.deleted) {
            ++active_count;
            if (!active_records.insert(node.record_id).second) {
                return std::unexpected(error(ErrorCode::CorruptedGraph, "HNSW graph has duplicate active record ids"));
            }
        }
        for (std::size_t layer = 0; layer < node.neighbors.size(); ++layer) {
            const auto limit = layer == 0 ? descriptor_.max_neighbors * 2 : descriptor_.max_neighbors;
            if (node.neighbors[layer].size() > limit) {
                return std::unexpected(error(ErrorCode::CorruptedGraph, "HNSW node exceeds its neighbor limit"));
            }
            std::unordered_set<HnswNodeId> unique;
            for (const auto neighbor_id : node.neighbors[layer]) {
                const auto neighbor = nodes.find(neighbor_id);
                if (neighbor_id == node_id || neighbor == nodes.end() || neighbor->second.level < layer ||
                    !unique.insert(neighbor_id).second) {
                    return std::unexpected(error(ErrorCode::CorruptedGraph, "HNSW neighbor reference is invalid"));
                }
            }
        }
    }
    if (maximum_node_id >= metadata.next_node_id || active_count != metadata.active_count) {
        return std::unexpected(error(ErrorCode::CorruptedGraph, "HNSW graph counters are invalid"));
    }
    return {};
}

bool HnswStore::same_descriptor(
    const HnswStoreDescriptor & left,
    const HnswStoreDescriptor & right
) noexcept
{
    return left.index_id == right.index_id &&
        left.collection_id == right.collection_id &&
        left.column_id == right.column_id &&
        left.dimension == right.dimension &&
        left.metric == right.metric &&
        left.max_neighbors == right.max_neighbors &&
        left.ef_construction == right.ef_construction &&
        left.ef_search_default == right.ef_search_default &&
        left.random_seed == right.random_seed;
}

} // namespace litedb::core::vindex::hnsw_index

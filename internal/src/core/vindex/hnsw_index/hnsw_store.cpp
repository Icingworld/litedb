#include "core/vindex/hnsw_index/hnsw_store.hpp"

#include <algorithm>
#include <cmath>
#include <limits>
#include <new>
#include <unordered_set>
#include <utility>

#include "core/filesystem/backend/filesystem_backend.hpp"

namespace litedb::core::vindex::hnsw_index
{

namespace
{

using Error = VectorIndexError;
using ErrorCode = VectorIndexErrorCode;

[[nodiscard]]
Error error(
    ErrorCode code,
    std::string message,
    VectorIndexOperation operation = VectorIndexOperation::Commit
)
{
    return Error {code, message, VectorIndexErrorContext {.operation = operation}};
}

[[nodiscard]]
Error filesystem_error(
    litedb::core::error::Error value,
    VectorIndexOperation operation,
    const std::filesystem::path & path
)
{
    return Error {
        ErrorCode::FileSystemFailure,
        value.message(),
        VectorIndexErrorContext {
            .operation = operation,
            .path = path,
            .source_code = value.encode_code(),
        },
    };
}

[[nodiscard]]
Error filesystem_error(litedb::core::error::Error value)
{
    return filesystem_error(std::move(value), VectorIndexOperation::Commit, {});
}

[[nodiscard]]
Error codec_error(VectorIndexError value)
{
    return value;
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

std::expected<HnswStore, VectorIndexError> HnswStore::create(
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

std::expected<HnswStore, VectorIndexError> HnswStore::open(
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

HnswStoreStats HnswStore::stats() const noexcept
{
    const auto estimated_node_bytes =
        64ULL + static_cast<std::uint64_t>(descriptor_.dimension) * sizeof(double) +
        static_cast<std::uint64_t>(descriptor_.max_neighbors) * 2ULL * sizeof(HnswNodeId);
    const auto maximum = std::numeric_limits<std::uint64_t>::max();
    const auto estimated_compact_bytes =
        metadata_.active_count > (maximum - HnswStoreCodec::HeaderSize) / estimated_node_bytes
            ? maximum
            : HnswStoreCodec::HeaderSize +
                static_cast<std::uint64_t>(metadata_.active_count) * estimated_node_bytes;
    return {
        .frame_count = metadata_.frame_sequence,
        .physical_node_count = nodes_.size(),
        .active_count = metadata_.active_count,
        .tombstone_count = nodes_.size() - metadata_.active_count,
        .file_bytes = file_size_bytes_,
        .estimated_compact_bytes = estimated_compact_bytes,
        .last_commit_upsert_count = last_commit_upsert_count_,
    };
}

std::expected<void, VectorIndexError> HnswStore::close()
{
    auto closed = file_.close();
    if (!closed) {
        return std::unexpected(filesystem_error(
            std::move(closed.error()),
            VectorIndexOperation::Publish,
            path_
        ));
    }
    return {};
}

std::expected<void, VectorIndexError> HnswStore::commit(
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

    auto valid = validate_mutation(metadata, upserts);
    if (!valid) {
        return std::unexpected(std::move(valid.error()));
    }

    auto encoded = HnswStoreCodec::encode_frame(HnswCommitFrame {
        .metadata = metadata,
        .upserts = upserts,
    }, descriptor_.dimension);
    if (!encoded) {
        return std::unexpected(codec_error(std::move(encoded.error())));
    }

    NodeMap staged_nodes;
    std::unordered_map<common::RecordId, HnswNodeId> staged_active;
    try {
        nodes_.reserve(nodes_.size() + upserts.size());
        active_nodes_.reserve(metadata.active_count);
        staged_nodes.reserve(upserts.size());
        staged_active.reserve(upserts.size());
        for (auto & node : upserts) {
            if (!node.deleted) {
                staged_active.emplace(node.record_id, node.node_id);
            }
            staged_nodes.emplace(node.node_id, std::move(node));
        }
    } catch (const std::bad_alloc &) {
        return std::unexpected(error(
            ErrorCode::ResourceLimitExceeded,
            "HNSW mutation preparation exceeded available memory"
        ));
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
    file_size_bytes_ = *previous_size + encoded->size();

    for (auto iterator = staged_nodes.begin(); iterator != staged_nodes.end();) {
        auto current = iterator++;
        const auto found = nodes_.find(current->first);
        if (found != nodes_.end()) {
            if (!found->second.deleted) {
                active_nodes_.erase(found->second.record_id);
            }
            found->second = std::move(current->second);
            staged_nodes.erase(current);
        }
    }
    nodes_.merge(staged_nodes);
    active_nodes_.merge(staged_active);
    metadata_ = metadata;
    last_commit_upsert_count_ = upserts.size();
    return {};
}

std::expected<void, VectorIndexError> HnswStore::load()
{
    auto file_size = file_.size();
    if (!file_size) {
        return std::unexpected(filesystem_error(std::move(file_size.error())));
    }
    if (*file_size < HnswStoreCodec::HeaderSize) {
        return std::unexpected(error(ErrorCode::CorruptedIndex, "HNSW store file is smaller than its header"));
    }
    HnswStoreCodec::HeaderBuffer header {};
    auto header_read = file_.read_at(0, header);
    if (!header_read) {
        return std::unexpected(filesystem_error(std::move(header_read.error())));
    }
    if (*header_read != header.size()) {
        return std::unexpected(error(ErrorCode::CorruptedIndex, "HNSW store header is truncated"));
    }
    auto decoded_header = HnswStoreCodec::decode_header(header);
    if (!decoded_header) {
        return std::unexpected(codec_error(std::move(decoded_header.error())));
    }
    if (!same_descriptor(*decoded_header, descriptor_)) {
        return std::unexpected(error(ErrorCode::CorruptedIndex, "HNSW store descriptor does not match catalog metadata"));
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
            return std::unexpected(error(ErrorCode::CorruptedIndex, "HNSW commit prefix is truncated"));
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
            return std::unexpected(error(ErrorCode::CorruptedIndex, "HNSW commit frame is truncated"));
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
    file_size_bytes_ = offset;
    return {};
}

std::expected<void, VectorIndexError> HnswStore::validate_mutation(
    const HnswGraphMetadata & metadata,
    const std::vector<HnswNode> & upserts
) const
{
    std::unordered_map<HnswNodeId, const HnswNode *> overlay;
    std::unordered_set<common::RecordId> upsert_records;
    overlay.reserve(upserts.size());
    for (const auto & node : upserts) {
        if (!overlay.emplace(node.node_id, &node).second) {
            return std::unexpected(error(ErrorCode::InvalidMutation, "HNSW commit mutates a node more than once"));
        }
        if (node.node_id == InvalidHnswNodeId || node.node_id >= metadata.next_node_id ||
            node.record_id == 0 || node.vector.size() != descriptor_.dimension ||
            node.level > 63 || node.neighbors.size() != node.level + 1) {
            return std::unexpected(error(ErrorCode::InvalidMutation, "HNSW mutation contains an invalid node"));
        }
        if (std::ranges::any_of(node.vector, [](double value) { return !std::isfinite(value); })) {
            return std::unexpected(error(ErrorCode::InvalidMutation, "HNSW mutation contains a non-finite vector"));
        }
        if (!node.deleted && !upsert_records.insert(node.record_id).second) {
            return std::unexpected(error(ErrorCode::InvalidMutation, "HNSW mutation duplicates an active record id"));
        }
    }

    const auto resolve = [&](HnswNodeId node_id) -> const HnswNode * {
        const auto changed = overlay.find(node_id);
        if (changed != overlay.end()) {
            return changed->second;
        }
        return find_node(node_id);
    };

    std::size_t active_count = metadata_.active_count;
    for (const auto & node : upserts) {
        const auto * previous = find_node(node.node_id);
        if (previous != nullptr && !previous->deleted) {
            --active_count;
        }
        if (!node.deleted) {
            ++active_count;
            const auto existing = active_nodes_.find(node.record_id);
            if (existing != active_nodes_.end() && existing->second != node.node_id) {
                const auto changed = overlay.find(existing->second);
                if (changed == overlay.end() || !changed->second->deleted) {
                    return std::unexpected(error(
                        ErrorCode::InvalidMutation,
                        "HNSW mutation conflicts with an active record id"
                    ));
                }
            }
        }
        for (std::size_t layer = 0; layer < node.neighbors.size(); ++layer) {
            const auto limit = layer == 0 ? descriptor_.max_neighbors * 2 : descriptor_.max_neighbors;
            if (node.neighbors[layer].size() > limit) {
                return std::unexpected(error(ErrorCode::InvalidMutation, "HNSW mutation exceeds a neighbor limit"));
            }
            std::unordered_set<HnswNodeId> unique;
            for (const auto neighbor_id : node.neighbors[layer]) {
                const auto * neighbor = resolve(neighbor_id);
                if (neighbor_id == node.node_id || neighbor == nullptr || neighbor->level < layer ||
                    !unique.insert(neighbor_id).second) {
                    return std::unexpected(error(
                        ErrorCode::InvalidMutation,
                        "HNSW mutation contains an invalid neighbor reference"
                    ));
                }
            }
        }
    }

    const auto * entry = resolve(metadata.entry_point);
    if (active_count != metadata.active_count ||
        (nodes_.empty() && upserts.empty()
            ? metadata.entry_point != InvalidHnswNodeId || metadata.max_level != 0 || metadata.active_count != 0
            : metadata.entry_point == InvalidHnswNodeId || entry == nullptr || entry->level != metadata.max_level)) {
        return std::unexpected(error(ErrorCode::InvalidMutation, "HNSW mutation metadata is inconsistent"));
    }
    return {};
}

std::expected<void, VectorIndexError> HnswStore::validate_graph(
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

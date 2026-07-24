#pragma once

#include <expected>
#include <filesystem>
#include <optional>
#include <string>
#include <unordered_map>
#include <vector>

#include "core/filesystem/file_handle.hpp"
#include "core/filesystem/filesystem.hpp"
#include "core/vindex/hnsw_index/hnsw_store_codec.hpp"

namespace litedb::core::vindex::hnsw_index
{

struct HnswStoreStats
{
    std::uint64_t frame_count {0};
    std::size_t physical_node_count {0};
    std::size_t active_count {0};
    std::size_t tombstone_count {0};
    std::uint64_t file_bytes {0};
    std::uint64_t estimated_compact_bytes {0};
    std::size_t last_commit_upsert_count {0};
};

class HnswStore final
{
public:
    using NodeMap = std::unordered_map<HnswNodeId, HnswNode>;

    HnswStore(const HnswStore &) = delete;
    HnswStore & operator=(const HnswStore &) = delete;
    HnswStore(HnswStore &&) noexcept = default;
    HnswStore & operator=(HnswStore &&) noexcept = default;

private:
    HnswStore(
        std::filesystem::path path,
        HnswStoreDescriptor descriptor,
        filesystem::FileHandle file
    ) noexcept;

public:
    [[nodiscard]]
    static std::expected<HnswStore, VectorIndexError> create(
        std::filesystem::path path,
        HnswStoreDescriptor descriptor,
        filesystem::FileSystem & filesystem
    );

    [[nodiscard]]
    static std::expected<HnswStore, VectorIndexError> open(
        std::filesystem::path path,
        const HnswStoreDescriptor & expected,
        filesystem::FileSystem & filesystem
    );

    [[nodiscard]]
    const std::filesystem::path & path() const noexcept;

    [[nodiscard]]
    const HnswStoreDescriptor & descriptor() const noexcept;

    [[nodiscard]]
    const HnswGraphMetadata & metadata() const noexcept;

    [[nodiscard]]
    const NodeMap & nodes() const noexcept;

    [[nodiscard]]
    const HnswNode * find_node(HnswNodeId node_id) const noexcept;

    [[nodiscard]]
    std::optional<HnswNodeId> find_active_node(common::RecordId record_id) const noexcept;

    [[nodiscard]]
    HnswStoreStats stats() const noexcept;

    [[nodiscard]]
    std::expected<void, VectorIndexError> close();

    [[nodiscard]]
    std::expected<void, VectorIndexError> commit(
        HnswGraphMetadata metadata,
        std::vector<HnswNode> upserts
    );

private:
    [[nodiscard]]
    std::expected<void, VectorIndexError> load();

    [[nodiscard]]
    std::expected<void, VectorIndexError> validate_graph(
        const NodeMap & nodes,
        const HnswGraphMetadata & metadata
    ) const;

    [[nodiscard]]
    std::expected<void, VectorIndexError> validate_mutation(
        const HnswGraphMetadata & metadata,
        const std::vector<HnswNode> & upserts
    ) const;

    [[nodiscard]]
    static bool same_descriptor(
        const HnswStoreDescriptor & left,
        const HnswStoreDescriptor & right
    ) noexcept;

private:
    std::filesystem::path path_;
    HnswStoreDescriptor descriptor_;
    filesystem::FileHandle file_;
    HnswGraphMetadata metadata_;
    NodeMap nodes_;
    std::unordered_map<common::RecordId, HnswNodeId> active_nodes_;
    std::uint64_t file_size_bytes_ {HnswStoreCodec::HeaderSize};
    std::size_t last_commit_upsert_count_ {0};
};

} // namespace litedb::core::vindex::hnsw_index

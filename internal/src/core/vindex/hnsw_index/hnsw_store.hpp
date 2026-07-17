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

enum class HnswStoreErrorCode
{
    FileSystemError,
    InvalidFormat,
    UnsupportedVersion,
    CorruptedGraph,
    InvalidMutation,
};

struct HnswStoreError
{
    HnswStoreErrorCode code;
    std::string message;
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
    static std::expected<HnswStore, HnswStoreError> create(
        std::filesystem::path path,
        HnswStoreDescriptor descriptor,
        filesystem::FileSystem & filesystem
    );

    [[nodiscard]]
    static std::expected<HnswStore, HnswStoreError> open(
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
    std::expected<void, HnswStoreError> commit(
        HnswGraphMetadata metadata,
        std::vector<HnswNode> upserts
    );

private:
    [[nodiscard]]
    std::expected<void, HnswStoreError> load();

    [[nodiscard]]
    std::expected<void, HnswStoreError> validate_graph(
        const NodeMap & nodes,
        const HnswGraphMetadata & metadata
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
};

} // namespace litedb::core::vindex::hnsw_index

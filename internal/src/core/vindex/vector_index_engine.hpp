#pragma once

#include <cstdint>
#include <expected>
#include <filesystem>
#include <optional>
#include <unordered_map>
#include <unordered_set>
#include <vector>

#include "core/common/ids.hpp"
#include "core/catalog/entry/vector_index_entry.hpp"
#include "core/schema/collection.hpp"
#include "core/common/record.hpp"
#include "core/vindex/vector_index_store.hpp"

namespace litedb::core::filesystem
{
class FileSystem;
}

namespace litedb::core::catalog
{
class CatalogViewer;
}

namespace litedb::core::storage
{
class StorageEngine;
}

namespace litedb::core::vindex
{

class HnswIndex;

/**
 * @brief 托管向量索引的只读运行时视图
 */
struct ManagedVectorIndexView
{
    common::VIndexId index_id;
    common::CollectionId collection_id;
    common::ColumnId column_id;
    std::size_t column_ordinal;
    VectorIndexKind kind;
    VectorDistanceMetric metric;
    std::size_t dimension;
    std::size_t entry_count;
};

struct VectorIndexKeyBinding
{
    common::VIndexId index_id;
    VectorIndexKey key;
};

struct VectorIndexUpdateBinding
{
    common::VIndexId index_id;
    std::optional<VectorIndexKey> old_key;
    std::optional<VectorIndexKey> new_key;
    bool key_changed {false};
};

using VectorIndexKeyBindings = std::vector<VectorIndexKeyBinding>;
using VectorIndexUpdateBindings = std::vector<VectorIndexUpdateBinding>;

struct VectorIndexMaintenanceStats
{
    std::uint64_t frame_count {0};
    std::size_t physical_node_count {0};
    std::size_t active_count {0};
    std::size_t tombstone_count {0};
    std::uint64_t file_bytes {0};
    std::uint64_t last_compaction_reclaimed_bytes {0};
    std::uint64_t last_compaction_duration_us {0};
};

/**
 * @brief 向量索引子系统入口
 * @details 负责多个运行时索引的生命周期、元数据解释、恢复策略、DML 维护与查询路由。
 */
class VectorIndexEngine
{
public:
    VectorIndexEngine(std::filesystem::path data_directory, filesystem::FileSystem & filesystem) noexcept;

    VectorIndexEngine(const VectorIndexEngine &) = delete;
    VectorIndexEngine & operator=(const VectorIndexEngine &) = delete;
    VectorIndexEngine(VectorIndexEngine &&) noexcept = default;
    VectorIndexEngine & operator=(VectorIndexEngine &&) noexcept = default;

    [[nodiscard]]
    std::expected<void, VectorIndexError> create_index(
        const catalog::entry::VectorIndexEntry & index_entry,
        const schema::CollectionSchema & collection_schema,
        const storage::StorageEngine & storage
    );

    /**
     * @brief 从权威元数据原子恢复全部向量索引
     * @note 任一索引失败时不发布部分状态，调用前的运行时状态保持不变。
     */
    [[nodiscard]]
    std::expected<void, VectorIndexError> restore_all(
        const catalog::CatalogViewer & catalog,
        const storage::StorageEngine & storage
    );

    /**
     * @brief 从正式索引文件原子刷新一个集合的全部向量索引
     */
    [[nodiscard]]
    std::expected<void, VectorIndexError> reload_collection(
        const catalog::CatalogViewer & catalog,
        const storage::StorageEngine & storage,
        common::CollectionId collection_id
    );

    [[nodiscard]] std::expected<void, VectorIndexError> drop_index(common::VIndexId index_id);
    [[nodiscard]] std::expected<void, VectorIndexError> drop_collection_indexes(common::CollectionId collection_id);
    [[nodiscard]] std::expected<void, VectorIndexError> checkpoint(const storage::StorageEngine & storage);

    [[nodiscard]] std::expected<VectorIndexKeyBindings, VectorIndexError> prepare_insert(
        common::CollectionId collection_id,
        const common::RecordData & record_data
    ) const;
    [[nodiscard]] std::expected<void, VectorIndexError> on_insert(
        common::RecordId record_id,
        const VectorIndexKeyBindings & bindings
    );
    [[nodiscard]] std::expected<VectorIndexUpdateBindings, VectorIndexError> prepare_update(
        common::CollectionId collection_id,
        const common::RecordData & old_record_data,
        const common::RecordData & new_record_data
    ) const;
    [[nodiscard]] std::expected<void, VectorIndexError> on_update(
        common::RecordId record_id,
        const VectorIndexUpdateBindings & bindings
    );
    [[nodiscard]] std::expected<VectorIndexKeyBindings, VectorIndexError> prepare_delete(
        common::CollectionId collection_id,
        const common::RecordData & old_record_data
    ) const;
    [[nodiscard]] std::expected<void, VectorIndexError> on_delete(
        common::RecordId record_id,
        const VectorIndexKeyBindings & bindings
    );

    [[nodiscard]] std::expected<std::vector<VectorSearchResult>, VectorIndexError> search(
        common::VIndexId index_id,
        const VectorIndexKey & query,
        VectorSearchRequest request
    ) const;

    [[nodiscard]] std::optional<ManagedVectorIndexView> find_index(common::VIndexId index_id) const noexcept;
    [[nodiscard]] std::vector<ManagedVectorIndexView> list_indexes(common::CollectionId collection_id) const;
    [[nodiscard]] VectorIndexMaintenanceStats maintenance_stats() const noexcept;

    void clear() noexcept;

private:
    [[nodiscard]] std::expected<void, VectorIndexError> insert(
        common::VIndexId index_id,
        const VectorIndexKey & key,
        common::RecordId record_id
    );
    [[nodiscard]] std::expected<void, VectorIndexError> erase(
        common::VIndexId index_id,
        common::RecordId record_id
    );
    [[nodiscard]] static std::expected<VectorIndexDescriptor, VectorIndexError> make_descriptor(
        const catalog::entry::VectorIndexEntry & index_entry,
        const schema::CollectionSchema & collection_schema
    );
    [[nodiscard]] std::expected<VectorIndexStore, VectorIndexError> create_store(
        const VectorIndexDescriptor & descriptor,
        const storage::StorageEngine & storage
    ) const;
    [[nodiscard]] std::expected<VectorIndexStore, VectorIndexError> restore_store(
        const VectorIndexDescriptor & descriptor,
        const storage::StorageEngine & storage
    ) const;
    [[nodiscard]] std::expected<VectorIndexStore, VectorIndexError> rebuild_store(
        const VectorIndexDescriptor & descriptor,
        const storage::StorageEngine & storage
    ) const;
    [[nodiscard]] std::expected<std::unique_ptr<VectorIndex>, VectorIndexError> make_backend(
        const VectorIndexDescriptor & descriptor,
        const storage::StorageEngine & storage,
        bool restore,
        std::filesystem::path path = {}
    ) const;
    [[nodiscard]] static std::expected<void, VectorIndexError> build_from_storage(
        VectorIndex & index,
        const VectorIndexDescriptor & descriptor,
        const storage::StorageEngine & storage
    );
    [[nodiscard]] static std::expected<void, VectorIndexError> verify_against_storage(
        const HnswIndex & index,
        const VectorIndexDescriptor & descriptor,
        const storage::StorageEngine & storage
    );

    [[nodiscard]] std::filesystem::path index_path(common::VIndexId index_id) const;
    [[nodiscard]] std::expected<void, VectorIndexError> cleanup_stale_temporary_files() const;
    [[nodiscard]] static ManagedVectorIndexView make_view(const VectorIndexStore & store) noexcept;
    [[nodiscard]] VectorIndexStore * find_store(common::VIndexId index_id) noexcept;
    [[nodiscard]] const VectorIndexStore * find_store(common::VIndexId index_id) const noexcept;
    void publish(VectorIndexStore store);
    void mark_recovery_required(common::VIndexId index_id) noexcept;

private:
    std::filesystem::path data_directory_;
    filesystem::FileSystem * filesystem_ {nullptr};
    std::unordered_map<common::VIndexId, VectorIndexStore> indexes_by_id_;
    std::unordered_map<common::CollectionId, std::vector<common::VIndexId>> indexes_by_collection_;
    std::unordered_set<common::CollectionId> dirty_collections_;
    std::uint64_t last_compaction_reclaimed_bytes_ {0};
    std::uint64_t last_compaction_duration_us_ {0};
};

} // namespace litedb::core::vindex

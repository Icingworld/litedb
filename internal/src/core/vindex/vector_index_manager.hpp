#pragma once

#include <cstddef>
#include <expected>
#include <filesystem>
#include <memory>
#include <optional>
#include <unordered_map>
#include <vector>

#include "core/common/ids.hpp"
#include "core/schema/record.hpp"
#include "core/vindex/vector_index.hpp"

namespace litedb::core::storage
{

class StorageEngine;

} // namespace litedb::core::storage

namespace litedb::core::filesystem
{

class FileSystem;

} // namespace litedb::core::filesystem

namespace litedb::core::vindex
{

class HnswIndex;

/**
 * @brief 向量索引定义
 */
struct VectorIndexDefinition
{
    common::VIndexId index_id;              ///< 向量索引 ID
    common::CollectionId collection_id;     ///< 集合 ID
    common::ColumnId column_id;             ///< 列 ID
    std::size_t column_ordinal {0};          ///< 列序号
    std::size_t dimension {0};               ///< 向量维度
    VectorIndexKind kind {VectorIndexKind::Flat};  ///< 索引类型
    VectorDistanceMetric metric {VectorDistanceMetric::L2}; ///< 距离度量
    std::size_t max_neighbors {16};
    std::size_t ef_construction {200};
    std::size_t ef_search_default {64};
    std::size_t random_seed {0};
};

/**
 * @brief 托管向量索引视图
 */
struct ManagedVectorIndexView
{
    common::VIndexId index_id;              ///< 向量索引 ID
    common::CollectionId collection_id;     ///< 集合 ID
    common::ColumnId column_id;             ///< 列 ID
    VectorIndexKind kind;                   ///< 索引类型
    const VectorIndex & index;              ///< 索引
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

/**
 * @brief 向量索引管理器
 */
class VectorIndexManager
{
public:
    explicit VectorIndexManager(const storage::StorageEngine & storage) noexcept;

    VectorIndexManager(
        std::filesystem::path data_directory,
        filesystem::FileSystem & filesystem,
        const storage::StorageEngine & storage
    ) noexcept;

public:
    [[nodiscard]]
    std::expected<void, VectorIndexError> create_index(const VectorIndexDefinition & definition);

    [[nodiscard]]
    std::expected<void, VectorIndexError> restore_index(const VectorIndexDefinition & definition);

    [[nodiscard]]
    std::expected<void, VectorIndexError> rebuild_index(const VectorIndexDefinition & definition);

    [[nodiscard]]
    std::expected<void, VectorIndexError> drop_index(common::VIndexId index_id);

    std::expected<void, VectorIndexError> drop_collection_indexes(common::CollectionId collection_id);

    [[nodiscard]]
    std::expected<void, VectorIndexError> insert(
        common::VIndexId index_id,
        const VectorIndexKey & key,
        common::RecordId record_id
    );

    [[nodiscard]]
    std::expected<void, VectorIndexError> erase(common::VIndexId index_id, common::RecordId record_id);

    [[nodiscard]]
    std::expected<VectorIndexKeyBindings, VectorIndexError> prepare_insert(
        common::CollectionId collection_id,
        const schema::RecordData & record_data
    ) const;

    [[nodiscard]]
    std::expected<void, VectorIndexError> on_insert(
        common::RecordId record_id,
        const VectorIndexKeyBindings & bindings
    );

    [[nodiscard]]
    std::expected<VectorIndexUpdateBindings, VectorIndexError> prepare_update(
        common::CollectionId collection_id,
        const schema::RecordData & old_record_data,
        const schema::RecordData & new_record_data
    ) const;

    [[nodiscard]]
    std::expected<void, VectorIndexError> on_update(
        common::RecordId record_id,
        const VectorIndexUpdateBindings & bindings
    );

    [[nodiscard]]
    std::expected<VectorIndexKeyBindings, VectorIndexError> prepare_delete(
        common::CollectionId collection_id,
        const schema::RecordData & old_record_data
    ) const;

    [[nodiscard]]
    std::expected<void, VectorIndexError> on_delete(
        common::RecordId record_id,
        const VectorIndexKeyBindings & bindings
    );

    [[nodiscard]]
    std::expected<std::vector<VectorSearchResult>, VectorIndexError> search(
        common::VIndexId index_id,
        const VectorIndexKey & query,
        VectorSearchRequest request
    ) const;

    [[nodiscard]]
    std::optional<ManagedVectorIndexView> find_index(common::VIndexId index_id) const noexcept;

    [[nodiscard]]
    std::vector<ManagedVectorIndexView> list_indexes(common::CollectionId collection_id) const;

    void clear() noexcept;

private:
    struct ManagedVectorIndex
    {
        common::VIndexId index_id;
        common::CollectionId collection_id;
        common::ColumnId column_id;
        std::size_t column_ordinal;
        VectorIndexKind kind;
        std::unique_ptr<VectorIndex> index;
    };

    [[nodiscard]]
    std::expected<std::unique_ptr<VectorIndex>, VectorIndexError> make_index(
        const VectorIndexDefinition & definition,
        bool restore
    ) const;

    [[nodiscard]]
    std::expected<void, VectorIndexError> build_from_storage(
        VectorIndex & index,
        const VectorIndexDefinition & definition
    ) const;

    [[nodiscard]]
    std::expected<void, VectorIndexError> verify_against_storage(
        const HnswIndex & index,
        const VectorIndexDefinition & definition
    ) const;

    [[nodiscard]]
    std::filesystem::path index_path(common::VIndexId index_id) const;

    [[nodiscard]]
    ManagedVectorIndexView make_view(const ManagedVectorIndex & managed_index) const noexcept;

    [[nodiscard]]
    ManagedVectorIndex * find_managed_index(common::VIndexId index_id) noexcept;

    [[nodiscard]]
    const ManagedVectorIndex * find_managed_index(common::VIndexId index_id) const noexcept;

private:
    std::filesystem::path data_directory_;
    filesystem::FileSystem * filesystem_ {nullptr};
    const storage::StorageEngine * storage_ {nullptr};
    std::unordered_map<common::VIndexId, ManagedVectorIndex> indexes_by_id_;
    std::unordered_map<common::CollectionId, std::vector<common::VIndexId>> indexes_by_collection_;
};

} // namespace litedb::core::vindex

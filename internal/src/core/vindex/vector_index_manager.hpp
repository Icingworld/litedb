#pragma once

#include <cstddef>
#include <expected>
#include <memory>
#include <optional>
#include <unordered_map>
#include <vector>

#include "core/common/ids.hpp"
#include "core/vindex/vector_index.hpp"

namespace litedb::core::storage
{

class StorageEngine;

} // namespace litedb::core::storage

namespace litedb::core::vindex
{

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

/**
 * @brief 向量索引管理器
 */
class VectorIndexManager
{
public:
    explicit VectorIndexManager(const storage::StorageEngine & storage) noexcept;

public:
    [[nodiscard]]
    std::expected<void, VectorIndexError> create_index(const VectorIndexDefinition & definition);

    [[nodiscard]]
    std::expected<void, VectorIndexError> drop_index(common::VIndexId index_id);

    void drop_collection_indexes(common::CollectionId collection_id);

    [[nodiscard]]
    std::expected<void, VectorIndexError> insert(
        common::VIndexId index_id,
        const VectorIndexKey & key,
        common::RecordId record_id
    );

    [[nodiscard]]
    std::expected<void, VectorIndexError> erase(common::VIndexId index_id, common::RecordId record_id);

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
        VectorIndexKind kind;
        std::unique_ptr<VectorIndex> index;
    };

    [[nodiscard]]
    std::unique_ptr<VectorIndex> make_index(const VectorIndexDefinition & definition) const;

    [[nodiscard]]
    ManagedVectorIndexView make_view(const ManagedVectorIndex & managed_index) const noexcept;

    [[nodiscard]]
    ManagedVectorIndex * find_managed_index(common::VIndexId index_id) noexcept;

    [[nodiscard]]
    const ManagedVectorIndex * find_managed_index(common::VIndexId index_id) const noexcept;

private:
    const storage::StorageEngine * storage_ {nullptr};
    std::unordered_map<common::VIndexId, ManagedVectorIndex> indexes_by_id_;
    std::unordered_map<common::CollectionId, std::vector<common::VIndexId>> indexes_by_collection_;
};

} // namespace litedb::core::vindex

#pragma once

#include <expected>
#include <memory>
#include <optional>
#include <unordered_map>
#include <vector>

#include "core/common/ids.hpp"
#include "core/vindex/hnsw_index.hpp"
#include "core/vindex/vector_index.hpp"

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
    VectorIndexKind kind {VectorIndexKind::Hnsw};  ///< 索引类型
    HnswIndexOptions hnsw_options;          ///< HNSW 配置
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
    VectorIndexManager() = default;

public:
    [[nodiscard]]
    std::expected<void, VectorIndexError> create_index(const VectorIndexDefinition & definition);

    [[nodiscard]]
    std::expected<void, VectorIndexError> drop_index(common::VIndexId index_id);

    void drop_collection_indexes(common::CollectionId collection_id);

    [[nodiscard]]
    std::expected<void, VectorIndexError> insert(
        common::VIndexId index_id,
        const schema::VectorValue & vector,
        common::RecordId record_id
    );

    [[nodiscard]]
    std::expected<void, VectorIndexError> erase(common::VIndexId index_id, common::RecordId record_id);

    [[nodiscard]]
    std::expected<void, VectorIndexError> update(
        common::VIndexId index_id,
        const schema::VectorValue & vector,
        common::RecordId record_id
    );

    [[nodiscard]]
    std::expected<std::vector<VectorSearchResult>, VectorIndexError> search(
        common::VIndexId index_id,
        const schema::VectorValue & query,
        VectorSearchParameters parameters
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
    static std::unique_ptr<VectorIndex> make_index(const VectorIndexDefinition & definition);

    [[nodiscard]]
    ManagedVectorIndexView make_view(const ManagedVectorIndex & managed_index) const noexcept;

    [[nodiscard]]
    ManagedVectorIndex * find_managed_index(common::VIndexId index_id) noexcept;

    [[nodiscard]]
    const ManagedVectorIndex * find_managed_index(common::VIndexId index_id) const noexcept;

private:
    std::unordered_map<common::VIndexId, ManagedVectorIndex> indexes_by_id_;
    std::unordered_map<common::CollectionId, std::vector<common::VIndexId>> indexes_by_collection_;
};

} // namespace litedb::core::vindex

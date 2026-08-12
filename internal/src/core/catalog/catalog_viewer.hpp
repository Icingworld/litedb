#pragma once

#include "core/catalog/catalog_state.hpp"

namespace litedb::core::catalog
{

// 目录查看器
// 离线或在线的目录状态都可以通过它进行查看
// 但查看内容仅在下一次目录发布前有效
class CatalogViewer
{
    friend class CatalogEditor;
    friend class CatalogPublisher;

private:
    explicit CatalogViewer(const CatalogState & state) noexcept;

public:
    // 按名称查找数据库
    [[nodiscard]]
    std::optional<const entry::DatabaseEntry &> find_database(std::string_view name) const;

    // 按 ID 查找数据库
    [[nodiscard]]
    std::optional<const entry::DatabaseEntry &> find_database(common::DatabaseId id) const;

    // 在指定数据库中按名称查找集合
    [[nodiscard]]
    std::optional<const entry::CollectionEntry &> find_collection(
        common::DatabaseId database_id,
        std::string_view name
    ) const;

    // 按 ID 查找集合
    [[nodiscard]]
    std::optional<const entry::CollectionEntry &> find_collection(common::CollectionId id) const;

    // 在指定集合中按名称查找列
    [[nodiscard]]
    std::optional<const entry::ColumnEntry &> find_column(
        common::CollectionId collection_id,
        std::string_view name
    ) const;

    // 按 ID 查找列
    [[nodiscard]]
    std::optional<const entry::ColumnEntry &> find_column(common::ColumnId id) const;

    // 在指定集合中按名称查找标量索引
    [[nodiscard]]
    std::optional<const entry::IndexEntry &> find_index(
        common::CollectionId collection_id,
        std::string_view name
    ) const;

    // 按 ID 查找标量索引
    [[nodiscard]]
    std::optional<const entry::IndexEntry &> find_index(common::IndexId id) const;

    // 在指定集合中按名称查找向量索引
    [[nodiscard]]
    std::optional<const entry::VectorIndexEntry &> find_vector_index(
        common::CollectionId collection_id,
        std::string_view name
    ) const;

    // 按 ID 查找向量索引
    [[nodiscard]]
    std::optional<const entry::VectorIndexEntry &> find_vector_index(common::VIndexId id) const;

    // 列出所有数据库
    [[nodiscard]]
    std::vector<std::reference_wrapper<const entry::DatabaseEntry>> list_databases() const;

    // 列出指定数据库下的所有集合
    [[nodiscard]]
    std::vector<std::reference_wrapper<const entry::CollectionEntry>> list_collections(
        common::DatabaseId database_id
    ) const;

    // 列出指定集合下的所有列
    [[nodiscard]]
    std::vector<std::reference_wrapper<const entry::ColumnEntry>> list_columns(
        common::CollectionId collection_id
    ) const;

    // 列出指定集合下的所有标量索引
    [[nodiscard]]
    std::vector<std::reference_wrapper<const entry::IndexEntry>> list_indexes(
        common::CollectionId collection_id
    ) const;

    // 列出指定集合下的所有向量索引
    [[nodiscard]]
    std::vector<std::reference_wrapper<const entry::VectorIndexEntry>> list_vector_indexes(
        common::CollectionId collection_id
    ) const;

    // 获取当前查看器对应的完整目录快照
    [[nodiscard]]
    CatalogSnapshot snapshot() const;

private:
    const CatalogState & state_;
};

} // namespace litedb::core::catalog

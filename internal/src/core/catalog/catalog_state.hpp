#pragma once

#include <expected>
#include <memory>
#include <string>
#include <string_view>
#include <unordered_map>
#include <vector>

#include "core/catalog/entry/collection_entry.hpp"
#include "core/catalog/entry/column_entry.hpp"
#include "core/catalog/entry/database_entry.hpp"
#include "core/catalog/entry/index_entry.hpp"
#include "core/catalog/entry/vector_index_entry.hpp"
#include "core/catalog/catalog_error.hpp"
#include "core/catalog/catalog_request.hpp"
#include "core/catalog/catalog_snapshot.hpp"

namespace litedb::core::catalog
{

// 目录状态
// 用于在内存中保存一份完整的目录快照
// 该状态不应被公开访问，而是通过 CatalogViewer 进行只读访问
// 通过 CatalogEditor 进行编辑，通过 CatalogPublisher 进行发布
class CatalogState
{
public:
    CatalogState() = default;

    CatalogState(const CatalogState &) = delete;

    CatalogState & operator=(const CatalogState &) = delete;

    CatalogState(CatalogState &&) noexcept = default;

    CatalogState & operator=(CatalogState &&) noexcept = default;

public:
    // 获取当前内存中的完整元数据快照
    [[nodiscard]]
    CatalogSnapshot snapshot() const;

private:
    friend class CatalogEditor;
    friend class CatalogPublisher;
    friend class CatalogViewer;
    friend std::expected<CatalogState, CatalogError> build_catalog_state(const CatalogSnapshot & snapshot);

    // 恢复元数据
    // 用快照替换当前内存状态，并校验快照结构与 ID 连续性
    [[nodiscard]]
    std::expected<void, CatalogError> restore(const CatalogSnapshot & snapshot);

public:
    // 按名称查找数据库
    [[nodiscard]]
    std::optional<const entry::DatabaseEntry &> find_database(std::string_view name) const;

    // 按 ID 查找数据库
    [[nodiscard]]
    std::optional<const entry::DatabaseEntry &> find_database(common::DatabaseId id) const;

    // 在指定数据库中按名称查找集合
    [[nodiscard]]
    std::optional<const entry::CollectionEntry &> find_collection(common::DatabaseId database_id, std::string_view name) const;

    // 按 ID 查找集合
    [[nodiscard]]
    std::optional<const entry::CollectionEntry &> find_collection(common::CollectionId id) const;

    // 在指定集合中按名称查找列
    [[nodiscard]]
    std::optional<const entry::ColumnEntry &> find_column(common::CollectionId collection_id, std::string_view name) const;

    // 按 ID 查找列
    [[nodiscard]]
    std::optional<const entry::ColumnEntry &> find_column(common::ColumnId id) const;

    // 在指定集合中按名称查找标量索引
    [[nodiscard]]
    std::optional<const entry::IndexEntry &> find_index(common::CollectionId collection_id, std::string_view name) const;

    // 按 ID 查找标量索引
    [[nodiscard]]
    std::optional<const entry::IndexEntry &> find_index(common::IndexId id) const;

    // 在指定集合中按名称查找向量索引
    [[nodiscard]]
    std::optional<const entry::VectorIndexEntry &> find_vector_index(common::CollectionId collection_id, std::string_view name) const;

    // 按 ID 查找向量索引
    [[nodiscard]]
    std::optional<const entry::VectorIndexEntry &> find_vector_index(common::VIndexId id) const;

    // 列出所有数据库
    [[nodiscard]]
    std::vector<std::reference_wrapper<const entry::DatabaseEntry>> list_databases() const;

    // 列出指定数据库下的所有集合
    [[nodiscard]]
    std::vector<std::reference_wrapper<const entry::CollectionEntry>> list_collections(common::DatabaseId database_id) const;

    // 列出指定集合下的所有列
    [[nodiscard]]
    std::vector<std::reference_wrapper<const entry::ColumnEntry>> list_columns(common::CollectionId collection_id) const;

    // 列出指定集合下的所有标量索引
    [[nodiscard]]
    std::vector<std::reference_wrapper<const entry::IndexEntry>> list_indexes(common::CollectionId collection_id) const;

    // 列出指定集合下的所有向量索引
    [[nodiscard]]
    std::vector<std::reference_wrapper<const entry::VectorIndexEntry>> list_vector_indexes(common::CollectionId collection_id) const;

private:
    // 创建数据库
    [[nodiscard]]
    std::expected<common::DatabaseId, CatalogError> create_database(const CreateDatabaseRequest & request);

    // 删除数据库
    // 会级联删除其下所有集合及相关元数据
    [[nodiscard]]
    std::expected<void, CatalogError> drop_database(const DropDatabaseRequest & request);

    // 创建集合
    [[nodiscard]]
    std::expected<common::CollectionId, CatalogError> create_collection(const CreateCollectionRequest & request);

    // 删除集合
    // 会级联删除其下所有列与索引
    [[nodiscard]]
    std::expected<void, CatalogError> drop_collection(const DropCollectionRequest & request);

    // 创建标量索引
    [[nodiscard]]
    std::expected<common::IndexId, CatalogError> create_index(const CreateIndexRequest & request);

    // 删除标量索引
    [[nodiscard]]
    std::expected<void, CatalogError> drop_index(const DropIndexRequest & request);

    // 创建向量索引
    [[nodiscard]]
    std::expected<common::VIndexId, CatalogError> create_vector_index(const CreateVectorIndexRequest & request);

    // 删除向量索引
    [[nodiscard]]
    std::expected<void, CatalogError> drop_vector_index(const DropVectorIndexRequest & request);

    // 按 ID 查找可修改的数据库项
    [[nodiscard]]
    std::optional<entry::DatabaseEntry &> find_database_mutable(common::DatabaseId id);

    // 按 ID 查找可修改的集合项
    [[nodiscard]]
    std::optional<entry::CollectionEntry &> find_collection_mutable(common::CollectionId id);

    // 删除集合并级联清理其列与索引
    void erase_collection(common::CollectionId id);

    // 删除集合前校验所有子项及反向索引
    [[nodiscard]]
    std::expected<void, CatalogError> validate_collection_for_erase(common::CollectionId id) const;

private:
    common::DatabaseId next_database_id_ {1};
    common::CollectionId next_collection_id_ {1};
    common::ColumnId next_column_id_ {1};
    common::IndexId next_index_id_ {1};
    common::VIndexId next_vector_index_id_ {1};

    std::vector<common::DatabaseId> database_ids_;                             
    std::unordered_map<common::DatabaseId, std::unique_ptr<entry::DatabaseEntry>> databases_;
    std::unordered_map<std::string, common::DatabaseId> database_keys_;

    std::unordered_map<common::CollectionId, std::unique_ptr<entry::CollectionEntry>> collections_;
    std::unordered_map<common::ColumnId, std::unique_ptr<entry::ColumnEntry>> columns_;
    std::unordered_map<common::IndexId, std::unique_ptr<entry::IndexEntry>> indexes_;
    std::unordered_map<common::VIndexId, std::unique_ptr<entry::VectorIndexEntry>> vector_indexes_;
};

// 从快照构建并完整验证目录状态
[[nodiscard]]
std::expected<CatalogState, CatalogError> build_catalog_state(const CatalogSnapshot & snapshot);

} // namespace litedb::core::catalog

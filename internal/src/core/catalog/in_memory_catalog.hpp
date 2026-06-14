#pragma once

#include <memory>
#include <string>
#include <unordered_map>
#include <vector>

#include "core/catalog/catalog.hpp"
#include "core/catalog/catalog_entry.hpp"
#include "core/catalog/catalog_snapshot.hpp"

namespace litedb::core::catalog
{

/**
 * @brief 内存目录
 */
class InMemoryCatalog final : public Catalog
{
public:
    InMemoryCatalog() = default;

public:
    /**
     * @brief 查找数据库
     * @param name 数据库名
     * @return 数据库
     */
    [[nodiscard]]
    const DatabaseEntry * find_database(std::string_view name) const override;

    /**
     * @brief 查找数据库
     * @param database_id 数据库 ID
     * @return 数据库
     */
    [[nodiscard]]
    const DatabaseEntry * find_database(common::DatabaseId database_id) const override;

    /**
     * @brief 查找集合
     * @param database_id 数据库 ID
     * @param name 集合名
     * @return 集合
     */
    [[nodiscard]]
    const CollectionEntry * find_collection(
        common::DatabaseId database_id,
        std::string_view name
    ) const override;

    /**
     * @brief 查找集合
     * @param collection_id 集合 ID
     * @return 集合
     */
    [[nodiscard]]
    const CollectionEntry * find_collection(common::CollectionId collection_id) const override;

    /**
     * @brief 查找列
     * @param collection_id 集合 ID
     * @param name 列名
     * @return 列
     */
    [[nodiscard]]
    const ColumnEntry * find_column(common::CollectionId collection_id, std::string_view name) const override;

    /**
     * @brief 查找列
     * @param column_id 列 ID
     * @return 列
     */
    [[nodiscard]]
    const ColumnEntry * find_column(common::ColumnId column_id) const override;

    /**
     * @brief 查找索引
     * @param collection_id 集合 ID
     * @param name 索引名
     * @return 索引
     */
    [[nodiscard]]
    const IndexEntry * find_index(
        common::CollectionId collection_id,
        std::string_view name
    ) const override;

    /**
     * @brief 查找索引
     * @param index_id 索引 ID
     * @return 索引
     */
    [[nodiscard]]
    const IndexEntry * find_index(common::IndexId index_id) const override;

    /**
     * @brief 查找向量索引
     * @param collection_id 集合 ID
     * @param name 向量索引名
     * @return 向量索引
     */
    [[nodiscard]]
    const VectorIndexEntry * find_vector_index(
        common::CollectionId collection_id,
        std::string_view name
    ) const override;

    /**
     * @brief 查找向量索引
     * @param index_id 向量索引 ID
     * @return 向量索引
     */
    [[nodiscard]]
    const VectorIndexEntry * find_vector_index(common::VIndexId index_id) const override;

    /**
     * @brief 列出所有数据库
     * @return 数据库列表
     */
    [[nodiscard]]
    std::vector<const DatabaseEntry *> list_databases() const override;

    /**
     * @brief 列出所有集合
     * @param database_id 数据库 ID
     * @return 集合列表
     */
    [[nodiscard]]
    std::vector<const CollectionEntry *> list_collections(common::DatabaseId database_id) const override;

    /**
     * @brief 列出所有列
     * @param collection_id 集合 ID
     * @return 列列表
     */
    [[nodiscard]]
    std::vector<const ColumnEntry *> list_columns(common::CollectionId collection_id) const override;

    /**
     * @brief 列出所有索引
     * @param collection_id 集合 ID
     * @return 索引列表
     */
    [[nodiscard]]
    std::vector<const IndexEntry *> list_indexes(common::CollectionId collection_id) const override;

    /**
     * @brief 列出所有向量索引
     * @param collection_id 集合 ID
     * @return 向量索引列表
     */
    [[nodiscard]]
    std::vector<const VectorIndexEntry *> list_vector_indexes(common::CollectionId collection_id) const override;

    /**
     * @brief 创建数据库
     * @param request 创建数据库请求
     * @return 数据库 ID
     */
    std::expected<common::DatabaseId, CatalogError> create_database(
        const CreateDatabaseRequest & request
    ) override;

    /**
     * @brief 删除数据库
     * @param request 删除数据库请求
     * @return 结果
     */
    std::expected<void, CatalogError> drop_database(const DropDatabaseRequest & request) override;

    /**
     * @brief 创建集合
     * @param request 创建集合请求
     * @return 集合 ID
     */
    std::expected<common::CollectionId, CatalogError> create_collection(
        const CreateCollectionRequest & request
    ) override;

    /**
     * @brief 删除集合
     * @param request 删除集合请求
     * @return 结果
     */
    std::expected<void, CatalogError> drop_collection(const DropCollectionRequest & request) override;

    /**
     * @brief 创建索引
     * @param request 创建索引请求
     * @return 索引 ID
     */
    std::expected<common::IndexId, CatalogError> create_index(const CreateIndexRequest & request) override;

    /**
     * @brief 删除索引
     * @param request 删除索引请求
     * @return 结果
     */
    std::expected<void, CatalogError> drop_index(const DropIndexRequest & request) override;

    /**
     * @brief 创建向量索引
     * @param request 创建向量索引请求
     * @return 向量索引 ID
     */
    std::expected<common::VIndexId, CatalogError> create_vector_index(
        const CreateVectorIndexRequest & request
    ) override;

    /**
     * @brief 删除向量索引
     * @param request 删除向量索引请求
     * @return 结果
     */
    std::expected<void, CatalogError> drop_vector_index(const DropVectorIndexRequest & request) override;

    [[nodiscard]]
    CatalogSnapshot snapshot() const;

    std::expected<void, CatalogError> restore(const CatalogSnapshot & snapshot);

private:
    /**
     * @brief 查找数据库
     * @param database_id 数据库 ID
     * @return 数据库
     */
    [[nodiscard]]
    DatabaseEntry * find_database_mutable(common::DatabaseId database_id);

    /**
     * @brief 查找集合
     * @param collection_id 集合 ID
     * @return 集合
     */
    [[nodiscard]]
    CollectionEntry * find_collection_mutable(common::CollectionId collection_id);

    /**
     * @brief 验证集合请求
     * @param request 集合请求
     * @return 结果
     */
    [[nodiscard]]
    std::expected<void, CatalogError> validate_collection_request(const CreateCollectionRequest & request) const;

    /**
     * @brief 验证索引请求
     * @param request 索引请求
     * @return 结果
     */
    [[nodiscard]]
    std::expected<void, CatalogError> validate_index_request(const CreateIndexRequest & request) const;

    /**
     * @brief 验证向量索引请求
     * @param request 向量索引请求
     * @return 结果
     */
    [[nodiscard]]
    std::expected<void, CatalogError> validate_vector_index_request(
        const CreateVectorIndexRequest & request
    ) const;

    /**
     * @brief 获取下一个数据库 ID
     * @return 下一个数据库 ID
     */
    [[nodiscard]]
    common::DatabaseId next_database_id() noexcept;

    /**
     * @brief 获取下一个集合 ID
     * @return 下一个集合 ID
     */
    [[nodiscard]]
    common::CollectionId next_collection_id() noexcept;

    /**
     * @brief 获取下一个列 ID
     * @return 下一个列 ID
     */
    [[nodiscard]]
    common::ColumnId next_column_id() noexcept;

    /**
     * @brief 获取下一个索引 ID
     * @return 下一个索引 ID
     */
    [[nodiscard]]
    common::IndexId next_index_id() noexcept;

    /**
     * @brief 获取下一个向量索引 ID
     * @return 下一个向量索引 ID
     */
    [[nodiscard]]
    common::VIndexId next_vector_index_id() noexcept;

private:
    common::DatabaseId next_database_id_ {1};           ///< 下一个数据库 ID
    common::CollectionId next_collection_id_ {1};       ///< 下一个集合 ID
    common::ColumnId next_column_id_ {1};               ///< 下一个列 ID
    common::IndexId next_index_id_ {1};                 ///< 下一个索引 ID
    common::VIndexId next_vector_index_id_ {1};          ///< 下一个向量索引 ID

    std::vector<common::DatabaseId> database_ids_;      ///< 数据库 ID 列表
    std::unordered_map<common::DatabaseId, std::unique_ptr<DatabaseEntry>> databases_by_id_;         ///< 数据库 ID 到数据库的映射
    std::unordered_map<std::string, common::DatabaseId> databases_by_key_;                           ///< 数据库键到数据库 ID 的映射

    std::unordered_map<common::CollectionId, std::unique_ptr<CollectionEntry>> collections_by_id_;   ///< 集合 ID 到集合的映射
    std::unordered_map<common::ColumnId, std::unique_ptr<ColumnEntry>> columns_by_id_;               ///< 列 ID 到列的映射
    std::unordered_map<common::IndexId, std::unique_ptr<IndexEntry>> indexes_by_id_;                 ///< 索引 ID 到索引的映射
    std::unordered_map<common::VIndexId, std::unique_ptr<VectorIndexEntry>> vector_indexes_by_id_;   ///< 向量索引 ID 到向量索引的映射
};

} // namespace litedb::core::catalog

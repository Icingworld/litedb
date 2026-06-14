#pragma once

#include <string_view>
#include <vector>

#include "core/common/ids.hpp"

namespace litedb::core::catalog
{

class ColumnEntry;
class CollectionEntry;
class DatabaseEntry;
class IndexEntry;
class VectorIndexEntry;

/**
 * @brief 目录读取器
 */
class CatalogReader
{
public:
    virtual ~CatalogReader() noexcept = default;

public:
    /**
     * @brief 查找数据库
     * @param name 数据库名
     * @return 数据库
     */
    [[nodiscard]]
    virtual const DatabaseEntry * find_database(std::string_view name) const = 0;

    /**
     * @brief 查找数据库
     * @param database_id 数据库 ID
     * @return 数据库
     */
    [[nodiscard]]
    virtual const DatabaseEntry * find_database(common::DatabaseId database_id) const = 0;

    /**
     * @brief 查找集合
     * @param database_id 数据库 ID
     * @param name 集合名
     * @return 集合
     */
    [[nodiscard]]
    virtual const CollectionEntry * find_collection(
        common::DatabaseId database_id,
        std::string_view name
    ) const = 0;

    /**
     * @brief 查找集合
     * @param collection_id 集合 ID
     * @return 集合
     */
    [[nodiscard]]
    virtual const CollectionEntry * find_collection(common::CollectionId collection_id) const = 0;

    /**
     * @brief 查找列
     * @param collection_id 集合 ID
     * @param name 列名
     * @return 列
     */
    [[nodiscard]]
    virtual const ColumnEntry * find_column(
        common::CollectionId collection_id,
        std::string_view name
    ) const = 0;

    /**
     * @brief 查找列
     * @param column_id 列 ID
     * @return 列
     */
    [[nodiscard]]
    virtual const ColumnEntry * find_column(common::ColumnId column_id) const = 0;

    /**
     * @brief 查找索引
     * @param collection_id 集合 ID
     * @param name 索引名
     * @return 索引
     */
    [[nodiscard]]
    virtual const IndexEntry * find_index(
        common::CollectionId collection_id,
        std::string_view name
    ) const = 0;

    /**
     * @brief 查找索引
     * @param index_id 索引 ID
     * @return 索引
     */
    [[nodiscard]]
    virtual const IndexEntry * find_index(common::IndexId index_id) const = 0;

    /**
     * @brief 查找向量索引
     * @param collection_id 集合 ID
     * @param name 向量索引名
     * @return 向量索引
     */
    [[nodiscard]]
    virtual const VectorIndexEntry * find_vector_index(
        common::CollectionId collection_id,
        std::string_view name
    ) const = 0;

    /**
     * @brief 查找向量索引
     * @param index_id 向量索引 ID
     * @return 向量索引
     */
    [[nodiscard]]
    virtual const VectorIndexEntry * find_vector_index(common::VIndexId index_id) const = 0;

    /**
     * @brief 列出所有数据库
     * @return 数据库列表
     */
    [[nodiscard]]
    virtual std::vector<const DatabaseEntry *> list_databases() const = 0;

    /**
     * @brief 列出所有集合
     * @param database_id 数据库 ID
     * @return 集合列表
     */
    [[nodiscard]]
    virtual std::vector<const CollectionEntry *> list_collections(common::DatabaseId database_id) const = 0;

    /**
     * @brief 列出所有列
     * @param collection_id 集合 ID
     * @return 列列表
     */
    [[nodiscard]]
    virtual std::vector<const ColumnEntry *> list_columns(common::CollectionId collection_id) const = 0;

    /**
     * @brief 列出所有索引
     * @param collection_id 集合 ID
     * @return 索引列表
     */
    [[nodiscard]]
    virtual std::vector<const IndexEntry *> list_indexes(common::CollectionId collection_id) const = 0;

    /**
     * @brief 列出所有向量索引
     * @param collection_id 集合 ID
     * @return 向量索引列表
     */
    [[nodiscard]]
    virtual std::vector<const VectorIndexEntry *> list_vector_indexes(common::CollectionId collection_id) const = 0;
};

} // namespace litedb::core::catalog

#pragma once

#include <cstddef>
#include <cstdint>
#include <optional>
#include <string>
#include <string_view>
#include <unordered_map>
#include <vector>

#include "core/catalog/catalog_default_expression.hpp"
#include "core/common/ids.hpp"
#include "core/common/logical_type.hpp"

namespace litedb::core::catalog
{

/**
 * @brief 目录项类型
 */
enum class CatalogEntryKind
{
    Database,             ///< 数据库
    Collection,           ///< 集合
    Column,               ///< 列
    Index,                ///< 索引
    VectorIndex,          ///< 向量索引
};

/**
 * @brief 索引类型
 */
enum class CatalogIndexKind
{
    Hash,                 ///< 哈希索引
    BTree,                ///< B+ 树索引
};

/**
 * @brief 向量索引类型
 */
enum class CatalogVectorIndexKind
{
    Hnsw,                 ///< HNSW 索引
};

/**
 * @brief 向量距离度量
 */
enum class CatalogVectorDistanceMetric
{
    L2,                   ///< 欧氏距离
    InnerProduct,         ///< 内积
    Cosine,               ///< 余弦距离
};

/**
 * @brief 规范化标识符
 * @param name 标识符
 * @return 规范化后的标识符
 */
[[nodiscard]]
std::string normalize_identifier(std::string_view name);

/**
 * @brief 目录项
 */
class CatalogEntry
{
public:
    /**
     * @brief 构造目录项
     * @param kind 目录项类型
     * @param id 目录项 ID
     * @param name 目录项名称
     */
    CatalogEntry(CatalogEntryKind kind, std::uint64_t id, std::string name);

    CatalogEntry(const CatalogEntry &) = delete;

    CatalogEntry & operator=(const CatalogEntry &) = delete;

    CatalogEntry(CatalogEntry &&) noexcept = default;

    CatalogEntry & operator=(CatalogEntry &&) noexcept = default;

    virtual ~CatalogEntry() noexcept = default;

public:
    /**
     * @brief 获取目录项类型
     * @return 目录项类型
     */
    [[nodiscard]]
    CatalogEntryKind kind() const noexcept;

    /**
     * @brief 获取目录项原始 ID
     * @return 目录项原始 ID
     */
    [[nodiscard]]
    std::uint64_t raw_id() const noexcept;

    /**
     * @brief 获取目录项名称
     * @return 目录项名称
     */
    [[nodiscard]]
    const std::string & name() const noexcept;

    /**
     * @brief 获取目录项键
     * @return 目录项键
     */
    [[nodiscard]]
    const std::string & key() const noexcept;

private:
    CatalogEntryKind kind_;     ///< 目录项类型
    std::uint64_t id_;          ///< 目录项原始 ID
    std::string name_;          ///< 目录项原始名称
    std::string key_;           ///< 目录项规范化后的键
};

/**
 * @brief 数据库目录项
 */
class DatabaseEntry final : public CatalogEntry
{
public:
    DatabaseEntry(common::DatabaseId id, std::string name);

public:
    /**
     * @brief 获取数据库 ID
     * @return 数据库 ID
     */
    [[nodiscard]]
    common::DatabaseId id() const noexcept;

    /**
     * @brief 获取数据库包含的集合 ID 列表
     * @return 数据库包含的集合 ID 列表
     */
    [[nodiscard]]
    const std::vector<common::CollectionId> & collection_ids() const noexcept;

    /**
     * @brief 查找集合 ID
     * @param collection_key 集合键
     * @return 集合 ID
     */
    [[nodiscard]]
    std::optional<common::CollectionId> find_collection_id(std::string_view collection_key) const;

    /**
     * @brief 添加集合
     * @param collection_key 集合键
     * @param collection_id 集合 ID
     */
    void add_collection(std::string_view collection_key, common::CollectionId collection_id);

    /**
     * @brief 删除集合
     * @param collection_key 集合键
     * @param collection_id 集合 ID
     */
    void remove_collection(std::string_view collection_key, common::CollectionId collection_id);

private:
    std::vector<common::CollectionId> collection_ids_;                          ///< 数据库包含的集合 ID 列表
    std::unordered_map<std::string, common::CollectionId> collections_by_key_;  ///< 数据库包含的集合键到 ID 的映射
};

/**
 * @brief 集合目录项
 */
class CollectionEntry final : public CatalogEntry
{
public:
    CollectionEntry(common::CollectionId id, common::DatabaseId database_id, std::string name, std::optional<std::string> comment = std::nullopt);

public:
    /**
     * @brief 获取集合 ID
     * @return 集合 ID
     */
    [[nodiscard]]
    common::CollectionId id() const noexcept;

    /**
     * @brief 获取数据库 ID
     * @return 数据库 ID
     */
    [[nodiscard]]
    common::DatabaseId database_id() const noexcept;

    /**
     * @brief 获取集合包含的列 ID 列表
     * @return 集合包含的列 ID 列表
     */
    [[nodiscard]]
    const std::vector<common::ColumnId> & column_ids() const noexcept;

    /**
     * @brief 获取集合包含的索引 ID 列表
     * @return 集合包含的索引 ID 列表
     */
    [[nodiscard]]
    const std::vector<common::IndexId> & index_ids() const noexcept;

    /**
     * @brief 获取集合包含的向量索引 ID 列表
     * @return 集合包含的向量索引 ID 列表
     */
    [[nodiscard]]
    const std::vector<common::VIndexId> & vector_index_ids() const noexcept;

    /**
     * @brief 获取集合主键列 ID
     * @return 集合主键列 ID
     */
    [[nodiscard]]
    std::optional<common::ColumnId> primary_key_column_id() const noexcept;

    /**
     * @brief 获取集合注释
     * @return 集合注释
     */
    [[nodiscard]]
    const std::optional<std::string> & comment() const noexcept;

    /**
     * @brief 查找列 ID
     * @param column_key 列键
     * @return 列 ID
     */
    [[nodiscard]]
    std::optional<common::ColumnId> find_column_id(std::string_view column_key) const;

    /**
     * @brief 查找索引 ID
     * @param index_key 索引键
     * @return 索引 ID
     */
    [[nodiscard]]
    std::optional<common::IndexId> find_index_id(std::string_view index_key) const;

    /**
     * @brief 查找向量索引 ID
     * @param index_key 向量索引键
     * @return 向量索引 ID
     */
    [[nodiscard]]
    std::optional<common::VIndexId> find_vector_index_id(std::string_view index_key) const;

    /**
     * @brief 添加列
     * @param column_key 列键
     * @param column_id 列 ID
     * @param primary_key 是否为主键
     */
    void add_column(std::string_view column_key, common::ColumnId column_id, bool primary_key);

    /**
     * @brief 添加索引
     * @param index_key 索引键
     * @param index_id 索引 ID
     */
    void add_index(std::string_view index_key, common::IndexId index_id);

    /**
     * @brief 添加向量索引
     * @param index_key 向量索引键
     * @param index_id 向量索引 ID
     */
    void add_vector_index(std::string_view index_key, common::VIndexId index_id);

    /**
     * @brief 删除索引
     * @param index_key 索引键
     * @param index_id 索引 ID
     */
    void remove_index(std::string_view index_key, common::IndexId index_id);

    /**
     * @brief 删除向量索引
     * @param index_key 向量索引键
     * @param index_id 向量索引 ID
     */
    void remove_vector_index(std::string_view index_key, common::VIndexId index_id);

private:
    common::DatabaseId database_id_;                                    ///< 数据库 ID
    std::vector<common::ColumnId> column_ids_;                          ///< 集合包含的列 ID 列表
    std::vector<common::IndexId> index_ids_;                            ///< 集合包含的索引 ID 列表
    std::vector<common::VIndexId> vector_index_ids_;                     ///< 集合包含的向量索引 ID 列表
    std::unordered_map<std::string, common::ColumnId> columns_by_key_;  ///< 集合包含的列键到 ID 的映射
    std::unordered_map<std::string, common::IndexId> indexes_by_key_;   ///< 集合包含的索引键到 ID 的映射
    std::unordered_map<std::string, common::VIndexId> vector_indexes_by_key_; ///< 集合包含的向量索引键到 ID 的映射
    std::optional<common::ColumnId> primary_key_column_id_;             ///< 集合主键列 ID
    std::optional<std::string> comment_;                                ///< 集合注释
};

/**
 * @brief 列目录项
 */
class ColumnEntry final : public CatalogEntry
{
public:
    /**
     * @brief 构造列目录项
     * @param id 列 ID
     * @param collection_id 集合 ID
     * @param name 列名称
     * @param type 列类型
     * @param primary_key 是否为主键
     * @param unique 是否唯一
     * @param nullable 是否可为空
     * @param default_expression 默认值表达式
     * @param comment 注释
     */
    ColumnEntry(
        common::ColumnId id,
        common::CollectionId collection_id,
        std::string name,
        common::LogicalType type,
        bool primary_key,
        bool unique,
        bool nullable,
        std::optional<CatalogDefaultExpression> default_expression,
        std::optional<std::string> comment
    );

public:
    /**
     * @brief 获取列 ID
     * @return 列 ID
     */
    [[nodiscard]]
    common::ColumnId id() const noexcept;

    /**
     * @brief 获取集合 ID
     * @return 集合 ID
     */
    [[nodiscard]]
    common::CollectionId collection_id() const noexcept;

    /**
     * @brief 获取列类型
     * @return 列类型
     */
    [[nodiscard]]
    const common::LogicalType & type() const noexcept;

    /**
     * @brief 是否为主键
     * @return 是否为主键
     */
    [[nodiscard]]
    bool primary_key() const noexcept;

    /**
     * @brief 是否唯一
     * @return 是否唯一
     */
    [[nodiscard]]
    bool unique() const noexcept;

    /**
     * @brief 是否可为空
     * @return 是否可为空
     */
    [[nodiscard]]
    bool nullable() const noexcept;

    /**
     * @brief 获取默认值表达式
     * @return 默认值表达式
     */
    [[nodiscard]]
    const std::optional<CatalogDefaultExpression> & default_expression() const noexcept;

    /**
     * @brief 获取注释
     * @return 注释
     */
    [[nodiscard]]
    const std::optional<std::string> & comment() const noexcept;

private:
    common::CollectionId collection_id_;    ///< 集合 ID
    common::LogicalType type_;              ///< 列类型
    bool primary_key_;                      ///< 是否为主键
    bool unique_;                           ///< 是否唯一
    bool nullable_;                         ///< 是否可为空
    std::optional<CatalogDefaultExpression> default_expression_;  ///< 默认值表达式
    std::optional<std::string> comment_;    ///< 注释
};

/**
 * @brief 索引目录项
 */
class IndexEntry final : public CatalogEntry
{
public:
    /**
     * @brief 构造索引目录项
     * @param id 索引 ID
     * @param collection_id 集合 ID
     * @param column_id 列 ID
     * @param name 索引名称
     * @param index_kind 索引类型
     * @param unique 是否唯一
     */
    IndexEntry(
        common::IndexId id,
        common::CollectionId collection_id,
        common::ColumnId column_id,
        std::string name,
        CatalogIndexKind index_kind,
        bool unique
    );

public:
    /**
     * @brief 获取索引 ID
     * @return 索引 ID
     */
    [[nodiscard]]
    common::IndexId id() const noexcept;

    /**
     * @brief 获取集合 ID
     * @return 集合 ID
     */
    [[nodiscard]]
    common::CollectionId collection_id() const noexcept;

    /**
     * @brief 获取列 ID
     * @return 列 ID
     */
    [[nodiscard]]
    common::ColumnId column_id() const noexcept;

    /**
     * @brief 获取索引类型
     * @return 索引类型
     */
    [[nodiscard]]
    CatalogIndexKind index_kind() const noexcept;

    /**
     * @brief 是否唯一
     * @return 是否唯一
     */
    [[nodiscard]]
    bool unique() const noexcept;

private:
    common::CollectionId collection_id_;    ///< 集合 ID
    common::ColumnId column_id_;            ///< 列 ID
    CatalogIndexKind index_kind_;           ///< 索引类型
    bool unique_;                           ///< 是否唯一
};

/**
 * @brief 向量索引目录项
 */
class VectorIndexEntry final : public CatalogEntry
{
public:
    /**
     * @brief 构造向量索引目录项
     * @param id 向量索引 ID
     * @param collection_id 集合 ID
     * @param column_id 列 ID
     * @param name 向量索引名称
     * @param index_kind 向量索引类型
     * @param metric 距离度量
     * @param dimension 向量维度
     * @param max_neighbors HNSW 最大邻居数量
     * @param ef_construction HNSW 构建候选数量
     * @param ef_search_default HNSW 默认搜索候选数量
     * @param random_seed 随机种子
     */
    VectorIndexEntry(
        common::VIndexId id,
        common::CollectionId collection_id,
        common::ColumnId column_id,
        std::string name,
        CatalogVectorIndexKind index_kind,
        CatalogVectorDistanceMetric metric,
        std::size_t dimension,
        std::size_t max_neighbors,
        std::size_t ef_construction,
        std::size_t ef_search_default,
        std::size_t random_seed
    );

public:
    /**
     * @brief 获取向量索引 ID
     * @return 向量索引 ID
     */
    [[nodiscard]]
    common::VIndexId id() const noexcept;

    /**
     * @brief 获取集合 ID
     * @return 集合 ID
     */
    [[nodiscard]]
    common::CollectionId collection_id() const noexcept;

    /**
     * @brief 获取列 ID
     * @return 列 ID
     */
    [[nodiscard]]
    common::ColumnId column_id() const noexcept;

    /**
     * @brief 获取向量索引类型
     * @return 向量索引类型
     */
    [[nodiscard]]
    CatalogVectorIndexKind index_kind() const noexcept;

    /**
     * @brief 获取距离度量
     * @return 距离度量
     */
    [[nodiscard]]
    CatalogVectorDistanceMetric metric() const noexcept;

    /**
     * @brief 获取向量维度
     * @return 向量维度
     */
    [[nodiscard]]
    std::size_t dimension() const noexcept;

    /**
     * @brief 获取 HNSW 最大邻居数量
     * @return HNSW 最大邻居数量
     */
    [[nodiscard]]
    std::size_t max_neighbors() const noexcept;

    /**
     * @brief 获取 HNSW 构建候选数量
     * @return HNSW 构建候选数量
     */
    [[nodiscard]]
    std::size_t ef_construction() const noexcept;

    /**
     * @brief 获取 HNSW 默认搜索候选数量
     * @return HNSW 默认搜索候选数量
     */
    [[nodiscard]]
    std::size_t ef_search_default() const noexcept;

    /**
     * @brief 获取随机种子
     * @return 随机种子
     */
    [[nodiscard]]
    std::size_t random_seed() const noexcept;

private:
    common::CollectionId collection_id_;            ///< 集合 ID
    common::ColumnId column_id_;                    ///< 列 ID
    CatalogVectorIndexKind index_kind_;             ///< 向量索引类型
    CatalogVectorDistanceMetric metric_;            ///< 距离度量
    std::size_t dimension_;                         ///< 向量维度
    std::size_t max_neighbors_;                     ///< HNSW 最大邻居数量
    std::size_t ef_construction_;                   ///< HNSW 构建候选数量
    std::size_t ef_search_default_;                 ///< HNSW 默认搜索候选数量
    std::size_t random_seed_;                       ///< 随机种子
};

} // namespace litedb::core::catalog

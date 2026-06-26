#pragma once

#include <expected>
#include <cstddef>
#include <optional>
#include <string>
#include <vector>

#include "core/catalog/catalog_default_expression.hpp"
#include "core/catalog/catalog_entry.hpp"
#include "core/catalog/catalog_error.hpp"
#include "core/common/ids.hpp"
#include "core/common/logical_type.hpp"

namespace litedb::core::catalog
{

/**
 * @brief 列定义
 */
struct ColumnDefinition
{
    std::string name;                   ///< 列名
    common::LogicalType type;           ///< 列类型
    bool primary_key {false};           ///< 是否为主键
    bool unique {false};                ///< 是否唯一
    bool nullable {true};               ///< 是否可为空
    std::optional<CatalogDefaultExpression> default_expression;     ///< 默认值
    std::optional<std::string> comment;                             ///< 注释
};

/**
 * @brief 创建数据库请求
 */
struct CreateDatabaseRequest
{
    std::string name;                   ///< 数据库名
    bool if_not_exists {false};         ///< 如果数据库不存在，则创建
};

/**
 * @brief 删除数据库请求
 */
struct DropDatabaseRequest
{
    std::string name;                   ///< 数据库名
    bool if_exists {false};             ///< 如果数据库存在，则删除
};

/**
 * @brief 创建集合请求
 */
struct CreateCollectionRequest
{
    common::DatabaseId database_id {0};     ///< 数据库 ID
    std::string name;                       ///< 集合名
    bool if_not_exists {false};             ///< 如果集合不存在，则创建
    std::vector<ColumnDefinition> columns;  ///< 列定义
    std::optional<std::string> comment;     ///< 集合注释
};

/**
 * @brief 删除集合请求
 */
struct DropCollectionRequest
{
    common::DatabaseId database_id {0};     ///< 数据库 ID
    std::string name;                       ///< 集合名
    bool if_exists {false};                 ///< 如果集合存在，则删除
};

/**
 * @brief 创建索引请求
 */
struct CreateIndexRequest
{
    common::CollectionId collection_id {0};     ///< 集合 ID
    common::ColumnId column_id {0};             ///< 列 ID
    std::string name;                           ///< 索引名
    CatalogIndexKind index_kind {CatalogIndexKind::BTree};  ///< 索引类型
    bool unique {false};                        ///< 是否唯一
    bool if_not_exists {false};                 ///< 如果索引不存在，则创建
};

/**
 * @brief 删除索引请求
 */
struct DropIndexRequest
{
    common::CollectionId collection_id {0};     ///< 集合 ID
    std::string name;                           ///< 索引名
    bool if_exists {false};                     ///< 如果索引存在，则删除
};

/**
 * @brief 创建向量索引请求
 */
struct CreateVectorIndexRequest
{
    common::CollectionId collection_id {0};     ///< 集合 ID
    common::ColumnId column_id {0};             ///< 列 ID
    std::string name;                           ///< 向量索引名
    CatalogVectorIndexKind index_kind {CatalogVectorIndexKind::Hnsw}; ///< 向量索引类型
    CatalogVectorDistanceMetric metric {CatalogVectorDistanceMetric::L2}; ///< 距离度量
    std::size_t max_neighbors {16};             ///< HNSW 最大邻居数量
    std::size_t ef_construction {200};          ///< HNSW 构建候选数量
    std::size_t ef_search_default {64};         ///< HNSW 默认搜索候选数量
    std::size_t random_seed {0};                ///< 随机种子
    bool if_not_exists {false};                 ///< 如果向量索引不存在，则创建
};

/**
 * @brief 删除向量索引请求
 */
struct DropVectorIndexRequest
{
    common::CollectionId collection_id {0};     ///< 集合 ID
    std::string name;                           ///< 向量索引名
    bool if_exists {false};                     ///< 如果向量索引存在，则删除
};

/**
 * @brief 目录写入器
 */
class CatalogWriter
{
public:
    virtual ~CatalogWriter() noexcept = default;

public:
    /**
     * @brief 创建数据库
     * @param request 创建数据库请求
     * @return 数据库 ID
     */
    virtual std::expected<common::DatabaseId, CatalogError> create_database(
        const CreateDatabaseRequest & request
    ) = 0;

    /**
     * @brief 删除数据库
     * @param request 删除数据库请求
     * @return 结果
     */
    virtual std::expected<void, CatalogError> drop_database(const DropDatabaseRequest & request) = 0;

    /**
     * @brief 创建集合
     * @param request 创建集合请求
     * @return 集合 ID
     */
    virtual std::expected<common::CollectionId, CatalogError> create_collection(
        const CreateCollectionRequest & request
    ) = 0;

    /**
     * @brief 删除集合
     * @param request 删除集合请求
     * @return 结果
     */
    virtual std::expected<void, CatalogError> drop_collection(const DropCollectionRequest & request) = 0;

    /**
     * @brief 创建索引
     * @param request 创建索引请求
     * @return 索引 ID
     */
    virtual std::expected<common::IndexId, CatalogError> create_index(const CreateIndexRequest & request) = 0;

    /**
     * @brief 删除索引
     * @param request 删除索引请求
     * @return 结果
     */
    virtual std::expected<void, CatalogError> drop_index(const DropIndexRequest & request) = 0;

    /**
     * @brief 创建向量索引
     * @param request 创建向量索引请求
     * @return 向量索引 ID
     */
    virtual std::expected<common::VIndexId, CatalogError> create_vector_index(
        const CreateVectorIndexRequest & request
    ) = 0;

    /**
     * @brief 删除向量索引
     * @param request 删除向量索引请求
     * @return 结果
     */
    virtual std::expected<void, CatalogError> drop_vector_index(const DropVectorIndexRequest & request) = 0;
};

} // namespace litedb::core::catalog

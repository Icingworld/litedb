#pragma once

#include <optional>
#include <string>
#include <vector>

#include "core/common/ids.hpp"
#include "core/common/logical_type.hpp"
#include "core/schema/default_expression.hpp"
#include "core/meta/entry/index_entry.hpp"
#include "core/meta/entry/vector_index_entry.hpp"

namespace litedb::core::meta
{

/**
 * @brief 列定义
 */
struct ColumnDefinition
{
    std::string name;
    common::LogicalType type;
    bool unique {false};
    bool nullable {true};
    std::optional<schema::DefaultExpression> default_expression;
    std::optional<std::string> comment;
};

/**
 * @brief 创建数据库请求
 */
struct CreateDatabaseRequest {
    std::string name;
    bool if_not_exists {false};
};

/**
 * @brief 删除数据库请求
 */
struct DropDatabaseRequest {
    std::string name;
    bool if_exists {false};
};

/**
 * @brief 创建集合请求
 */
struct CreateCollectionRequest
{
    common::DatabaseId database_id {0};
    std::string name;
    bool if_not_exists {false};
    std::vector<ColumnDefinition> columns;
    std::optional<std::string> comment;
};

/**
 * @brief 删除集合请求
 */
struct DropCollectionRequest
{
    common::DatabaseId database_id {0};
    std::string name;
    bool if_exists {false};
};

/**
 * @brief 创建索引请求
 */
struct CreateIndexRequest
{
    common::CollectionId collection_id {0};
    std::vector<common::ColumnId> column_ids;
    std::string name;
    entry::IndexKind kind {entry::IndexKind::BTree};
    bool unique {false};
    bool if_not_exists {false};
};

/**
 * @brief 删除索引请求
 */
struct DropIndexRequest
{
    common::CollectionId collection_id {0};
    std::string name;
    bool if_exists {false};
};

/**
 * @brief 创建向量索引请求
 */
struct CreateVectorIndexRequest
{
    common::CollectionId collection_id {0};
    common::ColumnId column_id {0};
    std::string name;
    entry::VectorIndexKind kind {entry::VectorIndexKind::Hnsw};
    entry::VectorDistanceMetric metric {entry::VectorDistanceMetric::L2};
    entry::HnswOptions hnsw_options;
    bool if_not_exists {false};
};

/**
 * @brief 删除向量索引请求
 */
struct DropVectorIndexRequest
{
    common::CollectionId collection_id {0};
    std::string name;
    bool if_exists {false};
};

} // namespace litedb::core::meta

#pragma once

#include <optional>
#include <string>
#include <vector>

#include "core/catalog/entry/index_entry.hpp"
#include "core/catalog/entry/vector_index_entry.hpp"
#include "core/common/ids.hpp"
#include "core/common/logical_type.hpp"
#include "core/schema/default_expression.hpp"

namespace litedb::core::catalog
{

// 列定义
struct ColumnDefinition
{
    std::string name;
    common::LogicalType type;
    bool unique {false};
    bool nullable {true};
    std::optional<schema::DefaultExpression> default_expression;
    std::optional<std::string> comment;
};

// 创建数据库请求
struct CreateDatabaseRequest
{
    std::string database_name;
};

// 删除数据库请求
struct DropDatabaseRequest
{
    common::DatabaseId database_id {0};
};

// 创建集合请求
struct CreateCollectionRequest
{
    common::DatabaseId database_id {0};
    std::string collection_name;
    std::vector<ColumnDefinition> columns;
    std::optional<std::string> comment {std::nullopt};
};

// 删除集合请求
struct DropCollectionRequest
{
    common::CollectionId collection_id {0};
};

// 创建索引请求
struct CreateIndexRequest
{
    common::CollectionId collection_id {0};
    common::ColumnId column_id {0};
    std::string index_name;
    entry::IndexKind kind {entry::IndexKind::BTree};
    bool unique {false};
};

// 删除索引请求
struct DropIndexRequest
{
    common::IndexId index_id {0};
};

// 创建向量索引请求
struct CreateVectorIndexRequest
{
    common::CollectionId collection_id {0};
    common::ColumnId column_id {0};
    std::string vector_index_name;
    entry::VectorIndexKind kind {entry::VectorIndexKind::Hnsw};
    entry::VectorDistanceMetric metric {entry::VectorDistanceMetric::L2};
    entry::HnswOptions hnsw_options;
};

// 删除向量索引请求
struct DropVectorIndexRequest
{
    common::VIndexId vector_index_id {0};
};

} // namespace litedb::core::catalog

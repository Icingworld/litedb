#pragma once

#include <optional>
#include <string>
#include <vector>

#include "core/common/ids.hpp"
#include "core/common/logical_type.hpp"
#include "core/schema/default_expression.hpp"
#include "core/catalog/entry/index_entry.hpp"
#include "core/catalog/entry/vector_index_entry.hpp"

namespace litedb::core::catalog
{

// 列快照
struct CatalogColumnSnapshot
{
    common::ColumnId id {0};
    std::string name;
    common::LogicalType type;
    bool unique {false};
    bool nullable {true};
    std::optional<schema::DefaultExpression> default_expression;
    std::optional<std::string> comment;
};

// 索引快照
struct CatalogIndexSnapshot
{
    common::IndexId id {0};
    common::ColumnId column_id {0};
    std::string name;
    entry::IndexKind index_kind {entry::IndexKind::BTree};
    bool unique {false};
};

// 向量索引快照
struct CatalogVectorIndexSnapshot
{
    common::VIndexId id {0};
    common::ColumnId column_id {0};
    std::string name;
    entry::VectorIndexKind index_kind {entry::VectorIndexKind::Hnsw};
    entry::VectorDistanceMetric metric {entry::VectorDistanceMetric::L2};
    std::size_t dimension {0};
    std::size_t max_neighbors {16};
    std::size_t ef_construction {200};
    std::size_t ef_search_default {64};
    std::size_t random_seed {0};
};

// 集合快照
struct CatalogCollectionSnapshot
{
    common::CollectionId id {0};
    common::DatabaseId database_id {0};
    std::string name;
    std::optional<std::string> comment;
    std::vector<CatalogColumnSnapshot> columns;
    std::vector<CatalogIndexSnapshot> indexes;
    std::vector<CatalogVectorIndexSnapshot> vector_indexes;
};

// 数据库快照
struct CatalogDatabaseSnapshot
{
    common::DatabaseId id {0};
    std::string name;
    std::vector<CatalogCollectionSnapshot> collections;
};

// 元数据快照
struct CatalogSnapshot
{
    common::DatabaseId next_database_id {1};
    common::CollectionId next_collection_id {1};
    common::ColumnId next_column_id {1};
    common::IndexId next_index_id {1};
    common::VIndexId next_vector_index_id {1};
    std::vector<CatalogDatabaseSnapshot> databases;
};

} // namespace litedb::core::catalog

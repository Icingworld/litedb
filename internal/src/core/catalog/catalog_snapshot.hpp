#pragma once

#include <optional>
#include <string>
#include <vector>

#include "core/catalog/catalog_default_expression.hpp"
#include "core/catalog/catalog_entry.hpp"
#include "core/common/ids.hpp"
#include "core/common/logical_type.hpp"

namespace litedb::core::catalog
{

struct CatalogSnapshotColumn
{
    common::ColumnId id {0};
    std::string name;
    common::LogicalType type;
    bool primary_key {false};
    bool unique {false};
    bool nullable {true};
    std::optional<CatalogDefaultExpression> default_expression;
    std::optional<std::string> comment;
};

struct CatalogSnapshotIndex
{
    common::IndexId id {0};
    common::ColumnId column_id {0};
    std::string name;
    CatalogIndexKind index_kind {CatalogIndexKind::BTree};
    bool unique {false};
};

struct CatalogSnapshotVectorIndex
{
    common::VIndexId id {0};
    common::ColumnId column_id {0};
    std::string name;
    CatalogVectorIndexKind index_kind {CatalogVectorIndexKind::Hnsw};
    CatalogVectorDistanceMetric metric {CatalogVectorDistanceMetric::L2};
    std::size_t dimension {0};
    std::size_t max_neighbors {16};
    std::size_t ef_construction {200};
    std::size_t ef_search_default {64};
    std::size_t random_seed {0};
};

struct CatalogSnapshotCollection
{
    common::CollectionId id {0};
    common::DatabaseId database_id {0};
    std::string name;
    std::optional<std::string> comment;
    std::vector<CatalogSnapshotColumn> columns;
    std::vector<CatalogSnapshotIndex> indexes;
    std::vector<CatalogSnapshotVectorIndex> vector_indexes;
};

struct CatalogSnapshotDatabase
{
    common::DatabaseId id {0};
    std::string name;
    std::vector<CatalogSnapshotCollection> collections;
};

struct CatalogSnapshot
{
    common::DatabaseId next_database_id {1};
    common::CollectionId next_collection_id {1};
    common::ColumnId next_column_id {1};
    common::IndexId next_index_id {1};
    common::VIndexId next_vector_index_id {1};
    std::vector<CatalogSnapshotDatabase> databases;
};

} // namespace litedb::core::catalog

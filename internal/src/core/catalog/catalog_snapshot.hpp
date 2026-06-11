#pragma once

#include <optional>
#include <string>
#include <vector>

#include "core/catalog/catalog_default_expression.hpp"
#include "core/common/ids.hpp"
#include "core/common/logical_id.hpp"

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

struct CatalogSnapshotCollection
{
    common::CollectionId id {0};
    common::DatabaseId database_id {0};
    std::string name;
    std::vector<CatalogSnapshotColumn> columns;
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
    std::vector<CatalogSnapshotDatabase> databases;
};

} // namespace litedb::core::catalog

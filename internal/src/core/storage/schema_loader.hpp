#pragma once

#include <expected>

#include "core/catalog/catalog_viewer.hpp"
#include "core/schema/collection.hpp"
#include "core/storage/schema_load_error.hpp"

namespace litedb::core::storage
{

/**
 * @brief 从 Catalog 加载集合 schema
 * @param catalog 目录读取器
 * @param collection_id 集合 ID
 * @return 集合 schema
 */
[[nodiscard]]
std::expected<schema::CollectionSchema, SchemaLoadError> load_collection_schema(
    const catalog::CatalogViewer & catalog,
    common::CollectionId collection_id
);

} // namespace litedb::core::storage

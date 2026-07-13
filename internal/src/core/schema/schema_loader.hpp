#pragma once

#include <expected>

#include "core/meta/meta_engine.hpp"
#include "core/schema/collection.hpp"
#include "core/schema/schema_error.hpp"

namespace litedb::core::schema
{

/**
 * @brief 加载集合 schema
 * @param catalog 目录读取器
 * @param collection_id 集合 ID
 * @return 集合 schema
 */
[[nodiscard]]
std::expected<CollectionSchema, SchemaError> load_collection_schema(
    const meta::MetaEngine & catalog,
    common::CollectionId collection_id
);

} // namespace litedb::core::schema

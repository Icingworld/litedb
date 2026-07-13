#include "core/schema/collection.hpp"

#include "core/meta/meta_helper.hpp"

#include <utility>

namespace litedb::core::schema
{

CollectionSchema::CollectionSchema(
    common::DatabaseId database_id,
    common::CollectionId collection_id,
    std::string collection_name,
    std::vector<ColumnSchema> columns,
    std::optional<std::string> comment
)
    : database_id_(database_id)
    , collection_id_(collection_id)
    , collection_name_(std::move(collection_name))
    , columns_(std::move(columns))
    , comment_(std::move(comment))
{
}

common::DatabaseId CollectionSchema::database_id() const noexcept
{
    return database_id_;
}

common::CollectionId CollectionSchema::collection_id() const noexcept
{
    return collection_id_;
}

const std::string & CollectionSchema::collection_name() const noexcept
{
    return collection_name_;
}

const std::optional<std::string> & CollectionSchema::comment() const noexcept
{
    return comment_;
}

const std::vector<ColumnSchema> & CollectionSchema::columns() const noexcept
{
    return columns_;
}

const ColumnSchema * CollectionSchema::column_at(std::size_t ordinal) const noexcept
{
    if (ordinal >= columns_.size()) {
        return nullptr;
    }
    return &columns_[ordinal];
}

const ColumnSchema * CollectionSchema::find_column(common::ColumnId column_id) const noexcept
{
    for (const auto & column : columns_) {
        if (column.column_id() == column_id) {
            return &column;
        }
    }
    return nullptr;
}

const ColumnSchema * CollectionSchema::find_column(std::string_view column_name) const
{
    const auto key = meta::normalize_identifier(column_name);
    for (const auto & column : columns_) {
        if (meta::normalize_identifier(column.column_name()) == key) {
            return &column;
        }
    }
    return nullptr;
}

} // namespace litedb::core::schema

#include "core/schema/column.hpp"

#include <utility>

namespace litedb::core::schema
{

ColumnSchema::ColumnSchema(
    common::ColumnId column_id,
    common::CollectionId collection_id,
    std::size_t ordinal,
    std::string column_name,
    common::LogicalType type,
    bool nullable,
    bool unique,
    std::optional<meta::entry::DefaultExpression> default_expression,
    std::optional<std::string> comment
)
    : column_id_(column_id)
    , collection_id_(collection_id)
    , ordinal_(ordinal)
    , column_name_(std::move(column_name))
    , type_(type)
    , nullable_(nullable)
    , unique_(unique)
    , default_expression_(std::move(default_expression))
    , comment_(std::move(comment))
{
}

common::ColumnId ColumnSchema::column_id() const noexcept
{
    return column_id_;
}

common::CollectionId ColumnSchema::collection_id() const noexcept
{
    return collection_id_;
}

std::size_t ColumnSchema::ordinal() const noexcept
{
    return ordinal_;
}

const std::string & ColumnSchema::column_name() const noexcept
{
    return column_name_;
}

const common::LogicalType & ColumnSchema::type() const noexcept
{
    return type_;
}

bool ColumnSchema::nullable() const noexcept
{
    return nullable_;
}

bool ColumnSchema::unique() const noexcept
{
    return unique_;
}

const std::optional<meta::entry::DefaultExpression> & ColumnSchema::default_expression() const noexcept
{
    return default_expression_;
}

const std::optional<std::string> & ColumnSchema::comment() const noexcept
{
    return comment_;
}

} // namespace litedb::core::schema

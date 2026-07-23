#include "core/meta/entry/column_entry.hpp"

#include <utility>

namespace litedb::core::meta::entry
{

ColumnEntry::ColumnEntry(
    common::ColumnId id,
    common::CollectionId collection_id,
    std::size_t ordinal,
    std::string name,
    common::LogicalType type,
    bool unique,
    bool nullable,
    std::optional<schema::DefaultExpression> default_expression,
    std::optional<std::string> comment
)
    : MetaEntry(MetaEntryKind::Column, id, std::move(name))
    , collection_id_(collection_id)
    , ordinal_(ordinal)
    , type_(std::move(type))
    , unique_(unique)
    , nullable_(nullable)
    , default_expression_(std::move(default_expression))
    , comment_(std::move(comment))
{
}

common::ColumnId ColumnEntry::id() const noexcept
{
    return raw_id();
}

common::CollectionId ColumnEntry::collection_id() const noexcept
{
    return collection_id_;
}

std::size_t ColumnEntry::ordinal() const noexcept
{
    return ordinal_;
}

const common::LogicalType & ColumnEntry::type() const noexcept
{
    return type_;
}

bool ColumnEntry::unique() const noexcept
{
    return unique_;
}

bool ColumnEntry::nullable() const noexcept
{
    return nullable_;
}

const std::optional<schema::DefaultExpression> & ColumnEntry::default_expression() const noexcept
{
    return default_expression_;
}

const std::optional<std::string> & ColumnEntry::comment() const noexcept
{
    return comment_;
}

} // namespace litedb::core::meta::entry

#include "core/catalog/entry/index_entry.hpp"

#include <utility>

namespace litedb::core::catalog::entry
{
    
IndexEntry::IndexEntry(
    common::IndexId id,
    common::CollectionId collection_id,
    common::ColumnId column_id,
    std::string name,
    IndexKind kind,
    bool unique
)
    : CatalogEntry(CatalogEntryKind::Index, id, std::move(name))
    , collection_id_(collection_id)
    , column_id_(column_id)
    , kind_(kind)
    , unique_(unique)
{
}

common::IndexId IndexEntry::id() const noexcept
{
    return raw_id();
}

IndexKind IndexEntry::kind() const noexcept
{
    return kind_;
}

common::CollectionId IndexEntry::collection_id() const noexcept
{
    return collection_id_;
}

common::ColumnId IndexEntry::column_id() const noexcept
{
    return column_id_;
}

bool IndexEntry::unique() const noexcept
{
    return unique_;
}

} // namespace litedb::core::catalog::entry

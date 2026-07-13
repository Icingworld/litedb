#include "core/meta/entry/index_entry.hpp"

#include <utility>

namespace litedb::core::meta::entry
{
    
IndexEntry::IndexEntry(
    common::IndexId id,
    common::CollectionId collection_id,
    std::vector<common::ColumnId> column_ids,
    std::string name,
    IndexKind kind,
    bool unique
)
    : MetaEntry(MetaEntryKind::Index, id, std::move(name))
    , collection_id_(collection_id)
    , column_ids_(std::move(column_ids))
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

std::optional<common::ColumnId> IndexEntry::column_id() const noexcept
{
    if (column_ids_.empty()) {
        return std::nullopt;
    }
    return column_ids_.front();
}

const std::vector<common::ColumnId> & IndexEntry::column_ids() const noexcept
{
    return column_ids_;
}

bool IndexEntry::unique() const noexcept
{
    return unique_;
}

} // namespace litedb::core::meta::entry

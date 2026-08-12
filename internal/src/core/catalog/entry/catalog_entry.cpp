#include "core/catalog/entry/catalog_entry.hpp"

#include "core/common/identifier.hpp"

#include <utility>

namespace litedb::core::catalog::entry
{

CatalogEntry::CatalogEntry(CatalogEntryKind kind, common::CatalogEntryId id, std::string name)
    : kind_(kind)
    , id_(id)
    , name_(std::move(name))
    , key_(common::normalize_identifier(name_))
{
}

CatalogEntryKind CatalogEntry::kind() const noexcept
{
    return kind_;
}

common::CatalogEntryId CatalogEntry::raw_id() const noexcept
{
    return id_;
}

const std::string & CatalogEntry::name() const noexcept
{
    return name_;
}

const std::string & CatalogEntry::key() const noexcept
{
    return key_;
}

} // namespace litedb::core::catalog::entry

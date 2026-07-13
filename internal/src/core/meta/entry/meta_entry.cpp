#include "core/meta/entry/meta_entry.hpp"

#include "core/meta/meta_helper.hpp"

#include <utility>

namespace litedb::core::meta::entry
{

MetaEntry::MetaEntry(MetaEntryKind kind, common::MetaEntryId id, std::string name)
    : kind_(kind)
    , id_(id)
    , name_(std::move(name))
    , key_(meta::normalize_identifier(name_))
{
}

MetaEntryKind MetaEntry::kind() const noexcept
{
    return kind_;
}

common::MetaEntryId MetaEntry::raw_id() const noexcept
{
    return id_;
}

const std::string & MetaEntry::name() const noexcept
{
    return name_;
}

const std::string & MetaEntry::key() const noexcept
{
    return key_;
}

} // namespace litedb::core::meta::entry

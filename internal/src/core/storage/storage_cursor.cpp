#include "core/storage/storage_cursor.hpp"

#include <optional>
#include <utility>

namespace litedb::core::storage
{

StorageCursor::StorageCursor(std::vector<common::Record> records) noexcept
    : records_(std::move(records))
    , position_(0)
{
}

std::expected<std::optional<common::Record>, StorageError> StorageCursor::next()
{
    if (position_ == records_.size()) {
        return std::nullopt;
    }
    return std::move(records_[position_++]);
}

} // namespace litedb::core::storage

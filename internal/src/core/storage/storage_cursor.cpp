#include "core/storage/storage_cursor.hpp"

#include <utility>

namespace litedb::core::storage
{

StorageCursor::StorageCursor(std::vector<common::Record> records) noexcept
    : records_(std::move(records))
{
}

std::expected<std::optional<common::Record>, StorageError> StorageCursor::next()
{
    if (position_ == records_.size()) {
        return std::optional<common::Record> {};
    }
    return std::optional<common::Record> {std::move(records_[position_++])};
}

} // namespace litedb::core::storage

#include "core/storage/storage_cursor.hpp"

#include <utility>

#include "core/storage/storage_store.hpp"

namespace litedb::core::storage
{

StorageCursor::StorageCursor(
    const StorageStore & store,
    std::vector<common::RecordId> record_ids
) noexcept
    : store_(&store)
    , record_ids_(std::move(record_ids))
    , position_(0)
{
}

std::expected<std::optional<common::Record>, StorageError> StorageCursor::next()
{
    if (position_ == record_ids_.size()) {
        return std::optional<common::Record> {};
    }

    auto record = store_->get(record_ids_[position_++]);
    if (!record.has_value()) {
        return std::unexpected(from_storage_store_error(std::move(record.error())));
    }
    return std::optional<common::Record> {std::move(record.value())};
}

} // namespace litedb::core::storage

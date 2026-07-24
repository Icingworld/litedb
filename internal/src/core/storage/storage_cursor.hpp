#pragma once

#include <cstddef>
#include <expected>
#include <optional>
#include <vector>

#include "core/common/record.hpp"
#include "core/storage/storage_error.hpp"

namespace litedb::core::storage
{

/**
 * @brief 拥有扫描快照的存储游标
 */
class StorageCursor
{
public:
    explicit StorageCursor(std::vector<common::Record> records) noexcept;

    StorageCursor(StorageCursor &&) noexcept = default;
    StorageCursor & operator=(StorageCursor &&) noexcept = default;
    StorageCursor(const StorageCursor &) = delete;
    StorageCursor & operator=(const StorageCursor &) = delete;

    [[nodiscard]]
    std::expected<std::optional<common::Record>, StorageError> next();

private:
    std::vector<common::Record> records_;
    std::size_t position_ {0};
};

} // namespace litedb::core::storage

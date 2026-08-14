#pragma once

#include <cstddef>
#include <expected>
#include <optional>
#include <vector>

#include "core/common/record.hpp"
#include "core/storage/storage_error.hpp"

namespace litedb::core::storage
{

// 拥有扫描快照的存储游标
// 储存已经从存储中读取到的所有记录，并逐个转移所有权给调用者
// 而不是逐步从存储中读取数据的游标
class StorageCursor
{
public:
    explicit StorageCursor(std::vector<common::Record> records) noexcept;

    StorageCursor(StorageCursor &&) noexcept = default;

    StorageCursor & operator=(StorageCursor &&) noexcept = default;

    StorageCursor(const StorageCursor &) = delete;

    StorageCursor & operator=(const StorageCursor &) = delete;

public:
    // 获取下一条记录
    [[nodiscard]]
    std::expected<std::optional<common::Record>, StorageError> next();

private:
    std::vector<common::Record> records_;
    std::size_t position_;
};

} // namespace litedb::core::storage

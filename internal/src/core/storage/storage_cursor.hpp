#pragma once

#include <cstddef>
#include <expected>
#include <optional>
#include <vector>

#include "core/common/ids.hpp"
#include "core/schema/record.hpp"
#include "core/storage/storage_error.hpp"

namespace litedb::core::storage
{

class StorageStore;

/**
 * @brief 持久化存储扫描游标
 */
class StorageCursor
{
public:
    StorageCursor(StorageCursor &&) noexcept = default;

    StorageCursor & operator=(StorageCursor &&) noexcept = default;

    StorageCursor(const StorageCursor &) = delete;

    StorageCursor & operator=(const StorageCursor &) = delete;

public:
    /**
     * @brief 获取下一个记录
     * @return 下一个记录
     */
    [[nodiscard]]
    std::expected<std::optional<schema::Record>, StorageError> next();

private:
    StorageCursor(const StorageStore & store, std::vector<common::RecordId> record_ids) noexcept;

private:
    const StorageStore * store_;                   ///< 持久化存储器
    std::vector<common::RecordId> record_ids_;     ///< 记录 ID 列表
    std::size_t position_;                         ///< 当前位置

    friend class StorageStore;
};

} // namespace litedb::core::storage

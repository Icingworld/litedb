#pragma once

#include <expected>
#include <memory>

#include "core/common/ids.hpp"
#include "core/schema/record.hpp"
#include "core/storage/record_cursor.hpp"
#include "core/storage/storage_error.hpp"

namespace litedb::core::storage
{

/**
 * @brief 集合存储
 */
class CollectionStorage
{
public:
    virtual ~CollectionStorage() noexcept = default;

public:
    /**
     * @brief 获取记录
     * @param record_id 记录 ID
     * @return 记录
     */
    [[nodiscard]]
    virtual std::expected<schema::Record, StorageError> get(common::RecordId record_id) const = 0;

    /**
     * @brief 插入记录
     * @param record_data 记录数据
     * @return 记录 ID
     */
    virtual std::expected<common::RecordId, StorageError> insert(schema::RecordData record_data) = 0;

    /**
     * @brief 更新记录
     * @param record_id 记录 ID
     * @param record_data 新记录数据
     * @return 结果
     */
    virtual std::expected<void, StorageError> update(
        common::RecordId record_id,
        schema::RecordData record_data
    ) = 0;

    /**
     * @brief 删除记录
     * @param record_id 记录 ID
     * @return 结果
     */
    virtual std::expected<void, StorageError> erase(common::RecordId record_id) = 0;

    /**
     * @brief 扫描记录
     * @return 记录游标
     */
    [[nodiscard]]
    virtual std::unique_ptr<RecordCursor> scan() const = 0;
};

} // namespace litedb::core::storage

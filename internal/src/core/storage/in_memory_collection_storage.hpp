#pragma once

#include <unordered_map>
#include <vector>

#include "core/schema/collection.hpp"
#include "core/storage/collection_storage.hpp"

namespace litedb::core::storage
{

/**
 * @brief 内存集合存储
 */
class InMemoryCollectionStorage final : public CollectionStorage
{
public:
    explicit InMemoryCollectionStorage(schema::CollectionSchema collection_schema);

public:
    /**
     * @brief 获取集合 schema
     * @return 集合 schema
     */
    [[nodiscard]]
    const schema::CollectionSchema & collection_schema() const noexcept;

    /**
     * @brief 获取记录
     * @param record_id 记录 ID
     * @return 记录
     */
    [[nodiscard]]
    std::expected<schema::Record, StorageError> get(common::RecordId record_id) const override;

    /**
     * @brief 插入记录
     * @param record_data 记录数据
     * @return 记录 ID
     */
    std::expected<common::RecordId, StorageError> insert(schema::RecordData record_data) override;

    /**
     * @brief 更新记录
     * @param record_id 记录 ID
     * @param record_data 新记录数据
     * @return 结果
     */
    std::expected<void, StorageError> update(
        common::RecordId record_id,
        schema::RecordData record_data
    ) override;

    /**
     * @brief 删除记录
     * @param record_id 记录 ID
     * @return 结果
     */
    std::expected<void, StorageError> erase(common::RecordId record_id) override;

    /**
     * @brief 扫描记录
     * @return 记录游标
     */
    [[nodiscard]]
    std::unique_ptr<RecordCursor> scan() const override;

private:
    /**
     * @brief 验证记录数据
     * @param record_data 记录数据
     * @return 结果
     */
    [[nodiscard]]
    std::expected<void, StorageError> validate_record(const schema::RecordData & record_data) const;

private:
    schema::CollectionSchema collection_schema_;                           ///< 集合 schema
    common::RecordId next_record_id_ {1};                                  ///< 下一个记录 ID
    std::vector<common::RecordId> record_ids_;                             ///< 记录 ID 顺序
    std::unordered_map<common::RecordId, schema::RecordData> records_;     ///< 记录数据
};

} // namespace litedb::core::storage

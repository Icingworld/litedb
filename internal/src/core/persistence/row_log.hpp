#pragma once

#include <cstdint>
#include <expected>
#include <filesystem>
#include <vector>

#include "core/common/ids.hpp"
#include "core/schema/record.hpp"
#include "core/storage/storage_error.hpp"

namespace litedb::core::persistence
{

/**
 * @brief 行日志操作类型
 */
enum class RowLogOperation : std::uint8_t
{
    Insert = 1,                     ///< 插入
    Update = 2,                     ///< 更新
    Delete = 3,                     ///< 删除
};

/**
 * @brief 行日志记录
 */
struct RowLogRecord
{
    RowLogOperation operation {RowLogOperation::Insert};  ///< 操作类型
    common::RecordId record_id {0};                       ///< 记录 ID
    schema::RecordData data;                              ///< 记录数据
};

/**
 * @brief 行日志回放
 */
struct RowLogReplay
{
    common::RecordId next_record_id {1};           ///< 下一个记录 ID
    std::vector<RowLogRecord> records;             ///< 记录列表
};

/**
 * @brief 行日志
 */
class RowLog
{
public:
    RowLog(std::filesystem::path path, common::CollectionId collection_id);

public:
    [[nodiscard]]
    std::expected<RowLogReplay, storage::StorageError> replay_or_create() const;

    [[nodiscard]]
    std::expected<void, storage::StorageError> append_insert(common::RecordId record_id, const schema::RecordData & data) const;

    [[nodiscard]]
    std::expected<void, storage::StorageError> append_update(common::RecordId record_id, const schema::RecordData & data) const;

    [[nodiscard]]
    std::expected<void, storage::StorageError> append_delete(common::RecordId record_id) const;

    [[nodiscard]]
    std::expected<void, storage::StorageError> mark_dropped() const;

    [[nodiscard]]
    const std::filesystem::path & path() const noexcept;

private:
    [[nodiscard]]
    std::expected<void, storage::StorageError> append(
        RowLogOperation operation,
        common::RecordId record_id,
        const schema::RecordData * data
    ) const;

private:
    std::filesystem::path path_;            ///< 路径
    common::CollectionId collection_id_;    ///< 集合 ID
};

} // namespace litedb::core::persistence

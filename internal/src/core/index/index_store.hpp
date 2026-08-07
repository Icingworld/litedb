#pragma once

#include <cstddef>
#include <expected>
#include <memory>
#include <vector>

#include "core/common/ids.hpp"
#include "core/common/logical_type.hpp"
#include "core/index/index_error.hpp"
#include "core/index/scalar_index.hpp"
#include "core/index/scalar_index_key.hpp"

namespace litedb::core::index
{

/**
 * @brief 单个索引实例的描述符
 */
struct IndexDescriptor
{
    common::IndexId index_id;               // 索引 ID
    common::CollectionId collection_id;     // 集合 ID
    common::ColumnId column_id;             // 列 ID
    std::size_t column_ordinal;             // 列序号
    common::LogicalType key_type;           // 键类型
    IndexKind kind;                         // 索引类型
    bool unique {false};                    // 是否唯一
};

/**
 * @brief 单个标量索引存储
 * @details 持有一个具体索引后端，并维护该索引的类型和唯一性约束。
 */
class IndexStore final
{
public:
    IndexStore(IndexDescriptor descriptor, std::unique_ptr<ScalarIndex> backend) noexcept;

    IndexStore(const IndexStore &) = delete;

    IndexStore & operator=(const IndexStore &) = delete;

    IndexStore(IndexStore &&) noexcept = default;

    IndexStore & operator=(IndexStore &&) noexcept = default;

public:
    /**
     * @brief 获取索引描述符
     */
    [[nodiscard]]
    const IndexDescriptor & descriptor() const noexcept;

    /**
     * @brief 在修改底层索引前校验待插入键
     */
    [[nodiscard]]
    std::expected<void, IndexError> validate_insert(const ScalarIndexKey & key) const;

    /**
     * @brief 插入键值对
     */
    [[nodiscard]]
    std::expected<void, IndexError> insert(const ScalarIndexKey & key, common::RecordId record_id);

    [[nodiscard]]
    std::expected<void, IndexError> bulk_load(std::vector<ScalarIndexEntry> entries);

    /**
     * @brief 删除键值对
     */
    [[nodiscard]]
    std::expected<void, IndexError> erase(const ScalarIndexKey & key, common::RecordId record_id);

    /**
     * @brief 查找等于给定键的记录 ID 列表
     */
    [[nodiscard]]
    std::expected<std::vector<common::RecordId>, IndexError> find_equal(const ScalarIndexKey & key) const;

    /**
     * @brief 扫描范围查询
     */
    [[nodiscard]]
    std::expected<std::vector<common::RecordId>, IndexError> scan_range(const IndexRange & range) const;

    [[nodiscard]]
    std::expected<std::unique_ptr<ScalarIndexCursor>, IndexError> scan_range_cursor(
        const IndexRange & range
    ) const;

    /**
     * @brief 获取索引大小
     */
    [[nodiscard]]
    std::size_t size() const noexcept;

private:
    /**
     * @brief 校验键是否合法
     */
    [[nodiscard]]
    std::expected<void, IndexError> validate_key(const ScalarIndexKey & key) const;

    /**
     * @brief 校验键是否唯一
     */
    [[nodiscard]]
    std::expected<void, IndexError> validate_unique(const ScalarIndexKey & key) const;

private:
    IndexDescriptor descriptor_;                // 索引描述符
    std::unique_ptr<ScalarIndex> backend_;      // 底层索引实现
};

} // namespace litedb::core::index

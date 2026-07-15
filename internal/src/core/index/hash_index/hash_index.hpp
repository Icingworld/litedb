#pragma once

#include <unordered_map>
#include <vector>

#include "core/index/scalar_index.hpp"

namespace litedb::core::index
{

/**
 * @brief 哈希索引
 */
class HashIndex final : public ScalarIndex
{
public:
    HashIndex();

public:
    /**
     * @brief 获取索引类型
     * @return 索引类型
     */
    [[nodiscard]]
    IndexKind kind() const noexcept override;

    /**
     * @brief 插入键值对
     * @param key 键
     * @param record_id 记录 ID
     * @return 是否成功
     */
    std::expected<void, IndexError> insert(
        const ScalarIndexKey & key,
        common::RecordId record_id
    ) override;

    /**
     * @brief 删除键值对
     * @param key 键
     * @param record_id 记录 ID
     * @return 是否成功
     */
    std::expected<void, IndexError> erase(
        const ScalarIndexKey & key,
        common::RecordId record_id
    ) override;

    /**
     * @brief 查找等于键的记录 ID
     * @param key 键
     * @return 记录 ID 列表
     */
    [[nodiscard]]
    std::expected<std::vector<common::RecordId>, IndexError> find_equal(
        const ScalarIndexKey & key
    ) const override;

    /**
     * @brief 清空索引
     */
    void clear() noexcept override;

    /**
     * @brief 获取索引大小
     * @return 索引大小
     */
    [[nodiscard]]
    std::size_t size() const noexcept override;

private:
    std::unordered_map<
        ScalarIndexKey,
        std::vector<common::RecordId>,
        ScalarIndexHash,
        ScalarIndexEqual
    > buckets_;                             ///< 桶
    std::size_t entry_count_;               ///< 键值对数量
};

} // namespace litedb::core::index

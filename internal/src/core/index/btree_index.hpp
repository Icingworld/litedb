#pragma once

#include <map>
#include <vector>

#include "core/index/scalar_index.hpp"

namespace litedb::core::index
{

/**
 * @brief B+ 树索引
 * @todo 实现 B+ 树索引，暂时使用 std::map 模拟 B+ 树
 */
class BTreeIndex final : public ScalarIndex
{
public:
    BTreeIndex();

public:
    /**
     * @brief 获取索引类型
     * @return 索引类型
     */
    [[nodiscard]]
    IndexKind kind() const noexcept override;

    /**
     * @brief 是否支持范围扫描
     * @return 是否支持范围扫描
     */
    [[nodiscard]]
    bool supports_range_scan() const noexcept override;

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
     * @brief 扫描 range 范围内的记录 ID
     * @param range 范围
     * @return 记录 ID 列表
     */
    [[nodiscard]]
    std::expected<std::vector<common::RecordId>, IndexError> scan_range(
        const IndexRange & range
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
    std::map<
        ScalarIndexKey, std::vector<common::RecordId>, ScalarIndexLess
    > buckets_;                             ///< 桶
    std::size_t entry_count_;               ///< 键值对数量
};

} // namespace litedb::core::index

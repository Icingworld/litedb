#pragma once

#include <cstddef>
#include <expected>
#include <optional>
#include <vector>

#include "core/common/ids.hpp"
#include "core/index/index_error.hpp"
#include "core/index/scalar_index_key.hpp"

namespace litedb::core::index
{

/**
 * @brief 索引类型
 */
enum class IndexKind
{
    Hash,                 ///< 哈希索引
    BTree,                ///< B树索引
};

/**
 * @brief 索引边界
 */
struct IndexBound
{
    ScalarIndexKey key;         ///< 键
    bool inclusive {true};      ///< 是否包含边界
};

/**
 * @brief 索引范围
 */
struct IndexRange
{
private:
    IndexRange(std::optional<IndexBound> lower, std::optional<IndexBound> upper);

public:
    /**
     * @brief 创建全范围索引
     * @return 全范围索引
     */
    [[nodiscard]]
    static IndexRange all();

    /**
     * @brief 创建闭合范围索引
     * @param lower 下界
     * @param upper 上界
     * @return 闭合范围索引
     */
    [[nodiscard]]
    static IndexRange closed(ScalarIndexKey lower, ScalarIndexKey upper);

    /**
     * @brief 创建任意开闭区间
     * @param lower 下界
     * @param lower_inclusive 下界是否包含
     * @param upper 上界
     * @param upper_inclusive 上界是否包含
     * @return 索引范围
     */
    [[nodiscard]]
    static IndexRange between(
        ScalarIndexKey lower,
        bool lower_inclusive,
        ScalarIndexKey upper,
        bool upper_inclusive
    );

    /**
     * @brief 创建下界范围索引
     * @param key 键
     * @param inclusive 是否包含边界
     * @return 下界范围索引
     */
    [[nodiscard]]
    static IndexRange lower_bound(ScalarIndexKey key, bool inclusive = true);

    /**
     * @brief 创建上界范围索引
     * @param key 键
     * @param inclusive 是否包含边界
     * @return 上界范围索引
     */
    [[nodiscard]]
    static IndexRange upper_bound(ScalarIndexKey key, bool inclusive = true);

    /**
     * @brief 获取下界
     * @return 下界
     */
    const std::optional<IndexBound> & lower() const noexcept;

    /**
     * @brief 获取上界
     * @return 上界
     */
    const std::optional<IndexBound> & upper() const noexcept;

private:
    std::optional<IndexBound> lower_;    ///< 下界
    std::optional<IndexBound> upper_;    ///< 上界
};

/**
 * @brief 标量索引
 */
class ScalarIndex
{
public:
    virtual ~ScalarIndex() noexcept = default;

public:
    /**
     * @brief 获取索引类型
     * @return 索引类型
     */
    [[nodiscard]]
    virtual IndexKind kind() const noexcept = 0;

    /**
     * @brief 插入键值对
     * @param key 键
     * @param record_id 记录 ID
     * @return 是否成功
     */
    virtual std::expected<void, IndexError> insert(
        const ScalarIndexKey & key,
        common::RecordId record_id
    ) = 0;

    /**
     * @brief 删除键值对
     * @param key 键
     * @param record_id 记录 ID
     * @return 是否成功
     */
    virtual std::expected<void, IndexError> erase(
        const ScalarIndexKey & key,
        common::RecordId record_id
    ) = 0;

    /**
     * @brief 查找等于键的记录 ID
     * @param key 键
     * @return 记录 ID 列表
     */
    [[nodiscard]]
    virtual std::expected<std::vector<common::RecordId>, IndexError> find_equal(
        const ScalarIndexKey & key
    ) const = 0;

    /**
     * @brief 获取索引大小
     * @return 索引大小
     */
    [[nodiscard]]
    virtual std::size_t size() const noexcept = 0;
};

/**
 * @brief 支持有序范围扫描的标量索引
 */
class OrderedScalarIndex : public ScalarIndex
{
public:
    ~OrderedScalarIndex() noexcept override = default;

public:
    /**
     * @brief 扫描 range 范围内的记录 ID
     * @param range 范围
     * @return 记录 ID 列表
     */
    [[nodiscard]]
    virtual std::expected<std::vector<common::RecordId>, IndexError> scan_range(
        const IndexRange & range
    ) const = 0;
};

} // namespace litedb::core::index

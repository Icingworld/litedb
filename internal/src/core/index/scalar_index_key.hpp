#pragma once

#include <compare>
#include <cstddef>
#include <expected>

#include "core/index/index_error.hpp"
#include "core/schema/value.hpp"

namespace litedb::core::index
{

/**
 * @brief 标量索引键
 */
class ScalarIndexKey
{
public:
    /**
     * @brief 从值创建标量索引键
     * @param value 值
     * @return 标量索引键
     */
    [[nodiscard]]
    static std::expected<ScalarIndexKey, IndexError> from_value(schema::Value value);

    /**
     * @brief 获取标量索引键值
     * @return 标量索引键值
     */
    [[nodiscard]]
    const schema::Value & value() const noexcept;

private:
    explicit ScalarIndexKey(schema::Value value);

private:
    schema::Value value_;       ///< 值
};

/**
 * @brief 标量索引键相等比较
 */
struct ScalarIndexEqual
{
    /**
     * @brief 比较两个标量索引键是否相等
     * @param left 左边的标量索引键
     * @param right 右边的标量索引键
     * @return 是否相等
     */
    [[nodiscard]]
    bool operator()(const ScalarIndexKey & left, const ScalarIndexKey & right) const noexcept;
};

/**
 * @brief 标量索引键哈希
 */
struct ScalarIndexHash
{
    /**
     * @brief 计算标量索引键的哈希值
     * @param key 标量索引键
     * @return 哈希值
     */
    [[nodiscard]]
    std::size_t operator()(const ScalarIndexKey & key) const noexcept;
};

/**
 * @brief 标量索引键小于比较
 */
struct ScalarIndexLess
{
    /**
     * @brief 比较左边标量索引键是否小于右边标量索引键
     * @param left 左边的标量索引键
     * @param right 右边的标量索引键
     * @return 是否小于
     */
    [[nodiscard]]
    bool operator()(const ScalarIndexKey & left, const ScalarIndexKey & right) const noexcept;
};

/**
 * @brief 按精确物理类型比较两个标量索引键
 * @param left 左边的标量索引键
 * @param right 右边的标量索引键
 * @return 比较结果
 */
[[nodiscard]]
std::strong_ordering compare_scalar_index_keys(
    const ScalarIndexKey & left,
    const ScalarIndexKey & right
) noexcept;

} // namespace litedb::core::index

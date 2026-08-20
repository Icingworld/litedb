#pragma once

#include <compare>
#include <cstddef>
#include <expected>

#include "core/index/index_error.hpp"
#include "core/common/value.hpp"

namespace litedb::core::index
{

// 标量索引键
class ScalarIndexKey
{
public:
    // 从值创建标量索引键
    [[nodiscard]]
    static std::expected<ScalarIndexKey, IndexError> from_value(common::Value value);

    // 获取标量索引键值
    [[nodiscard]]
    const common::Value & value() const noexcept;

private:
    explicit ScalarIndexKey(common::Value value);

private:
    common::Value value_;       // 值
};

// 标量索引键相等比较
struct ScalarIndexEqual
{
    // 比较两个标量索引键是否相等
    [[nodiscard]]
    bool operator()(const ScalarIndexKey & left, const ScalarIndexKey & right) const noexcept;
};

// 标量索引键哈希
struct ScalarIndexHash
{
    // 计算标量索引键的哈希值
    [[nodiscard]]
    std::size_t operator()(const ScalarIndexKey & key) const noexcept;
};

// 标量索引键小于比较
struct ScalarIndexLess
{
    // 比较左边标量索引键是否小于右边标量索引键
    [[nodiscard]]
    bool operator()(const ScalarIndexKey & left, const ScalarIndexKey & right) const noexcept;
};

// 按精确物理类型比较两个标量索引键
[[nodiscard]]
std::strong_ordering compare_scalar_index_keys(
    const ScalarIndexKey & left,
    const ScalarIndexKey & right
) noexcept;

} // namespace litedb::core::index

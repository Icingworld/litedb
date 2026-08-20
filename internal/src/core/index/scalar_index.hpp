#pragma once

#include <cstddef>
#include <expected>
#include <memory>
#include <optional>
#include <vector>

#include "core/common/ids.hpp"
#include "core/index/index_error.hpp"
#include "core/index/scalar_index_key.hpp"

namespace litedb::core::index
{

// 索引类型
enum class IndexKind
{
    BTree = 0,            // B+ 树索引
};

// 索引边界
struct IndexBound
{
    ScalarIndexKey key;         // 键
    bool inclusive {true};      // 是否包含边界
};

// 索引范围
struct IndexRange
{
private:
    IndexRange(std::optional<IndexBound> lower, std::optional<IndexBound> upper);

public:
    // 创建全范围索引
    [[nodiscard]]
    static IndexRange all();

    // 创建闭合范围索引
    [[nodiscard]]
    static IndexRange closed(ScalarIndexKey lower, ScalarIndexKey upper);

    // 创建任意开闭区间
    [[nodiscard]]
    static IndexRange between(
        ScalarIndexKey lower,
        bool lower_inclusive,
        ScalarIndexKey upper,
        bool upper_inclusive
    );

    // 创建下界范围索引
    [[nodiscard]]
    static IndexRange lower_bound(ScalarIndexKey key, bool inclusive = true);

    // 创建上界范围索引
    [[nodiscard]]
    static IndexRange upper_bound(ScalarIndexKey key, bool inclusive = true);

    // 获取下界
    const std::optional<IndexBound> & lower() const noexcept;

    // 获取上界
    const std::optional<IndexBound> & upper() const noexcept;

private:
    std::optional<IndexBound> lower_;    // 下界
    std::optional<IndexBound> upper_;    // 上界
};

struct ScalarIndexEntry
{
    ScalarIndexKey key;
    common::RecordId record_id;
};

class ScalarIndexCursor
{
public:
    virtual ~ScalarIndexCursor() noexcept = default;

    [[nodiscard]]
    virtual std::expected<std::optional<common::RecordId>, IndexError> next() = 0;
};

// 标量索引的最小能力接口
// 只要求精确键查询，不假设键有序。当前正式后端只有 B+Tree；保留该层是为了以后接入位图索引、倒排索引或其他仅支持等值/集合检索、不适合范围扫描的标量索引实现。
class ScalarIndex
{
public:
    virtual ~ScalarIndex() noexcept = default;

public:
    // 获取索引类型
    [[nodiscard]]
    virtual IndexKind kind() const noexcept = 0;

    // 插入键值对
    virtual std::expected<void, IndexError> insert(
        const ScalarIndexKey & key,
        common::RecordId record_id
    ) = 0;

    // 删除键值对
    virtual std::expected<void, IndexError> erase(
        const ScalarIndexKey & key,
        common::RecordId record_id
    ) = 0;

    // 查找等于键的记录 ID
    [[nodiscard]]
    virtual std::expected<std::vector<common::RecordId>, IndexError> find_equal(
        const ScalarIndexKey & key
    ) const = 0;

    // 获取索引大小
    [[nodiscard]]
    virtual std::size_t size() const noexcept = 0;

    // 扫描 range 范围内的记录 ID
    // 不支持有序范围扫描的后端返回 UnsupportedRangeScan。
    [[nodiscard]]
    virtual std::expected<std::vector<common::RecordId>, IndexError> scan_range(
        const IndexRange & range
    ) const;

    [[nodiscard]]
    virtual std::expected<std::unique_ptr<ScalarIndexCursor>, IndexError> scan_range_cursor(
        const IndexRange & range
    ) const;

    // 在空索引上批量构建条目
    // 默认实现逐条插入；持久化有序后端可以覆盖为线性构建。
    virtual std::expected<void, IndexError> bulk_load(std::vector<ScalarIndexEntry> entries);
};

// 支持有序范围扫描的标量索引
// 有序后端通过该派生接口显式声明范围能力，调用方无需在基础接口中假定所有索引都可排序。
class OrderedScalarIndex : public ScalarIndex
{
public:
    ~OrderedScalarIndex() noexcept override = default;

public:
    // 扫描 range 范围内的记录 ID
    [[nodiscard]]
    virtual std::expected<std::vector<common::RecordId>, IndexError> scan_range(
        const IndexRange & range
    ) const = 0;

    [[nodiscard]]
    virtual std::expected<std::unique_ptr<ScalarIndexCursor>, IndexError> scan_range_cursor(
        const IndexRange & range
    ) const = 0;
};

} // namespace litedb::core::index

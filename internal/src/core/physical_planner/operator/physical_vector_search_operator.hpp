#pragma once

#include <cstddef>
#include <memory>
#include <optional>

#include "core/binder/bound/expression/bound_expression.hpp"
#include "core/common/ids.hpp"
#include "core/meta/entry/vector_index_entry.hpp"
#include "core/physical_planner/operator/physical_operator.hpp"

namespace litedb::core::physical_planner::op
{

// 向量检索算子
class VectorSearchOperator final : public PhysicalOperator
{
public:
    VectorSearchOperator(
        common::CollectionId collection_id,
        common::VIndexId index_id,
        common::ColumnId column_id,
        meta::entry::VectorDistanceMetric metric,
        std::unique_ptr<binder::bound::BoundExpression> query_vector,
        std::unique_ptr<binder::bound::BoundExpression> predicate,
        std::size_t required_count
    ) noexcept;

public:
    // 获取集合 ID
    [[nodiscard]]
    common::CollectionId collection_id() const noexcept;

    // 获取向量索引 ID
    [[nodiscard]]
    common::VIndexId index_id() const noexcept;

    // 获取列 ID
    [[nodiscard]]
    common::ColumnId column_id() const noexcept;

    // 获取距离度量
    [[nodiscard]]
    meta::entry::VectorDistanceMetric metric() const noexcept;

    // 获取查询向量表达式
    [[nodiscard]]
    const binder::bound::BoundExpression & query_vector() const noexcept;

    // 获取残差谓词
    [[nodiscard]]
    std::optional<const binder::bound::BoundExpression &>
    predicate() const noexcept;

    // 获取所需结果数量
    [[nodiscard]]
    std::size_t required_count() const noexcept;

private:
    common::CollectionId collection_id_;
    common::VIndexId index_id_;
    common::ColumnId column_id_;
    meta::entry::VectorDistanceMetric metric_;
    std::unique_ptr<binder::bound::BoundExpression> query_vector_;
    std::unique_ptr<binder::bound::BoundExpression> predicate_;
    std::size_t required_count_;
};

} // namespace litedb::core::physical_planner::op

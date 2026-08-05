#pragma once

#include <cstddef>
#include <memory>
#include <utility>

#include "core/binder/bound/expression/bound_expression.hpp"
#include "core/common/ids.hpp"
#include "core/meta/entry/vector_index_entry.hpp"
#include "core/physical_planner/operator/physical_operator.hpp"

namespace litedb::core::physical_planner::op
{

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
    ) noexcept
        : PhysicalOperator(PhysicalOperatorKind::VectorSearch)
        , collection_id_(collection_id)
        , index_id_(index_id)
        , column_id_(column_id)
        , metric_(metric)
        , query_vector_(std::move(query_vector))
        , predicate_(std::move(predicate))
        , required_count_(required_count)
    {
    }

    [[nodiscard]] common::CollectionId collection_id() const noexcept
    {
        return collection_id_;
    }

    [[nodiscard]] common::VIndexId index_id() const noexcept
    {
        return index_id_;
    }

    [[nodiscard]] common::ColumnId column_id() const noexcept
    {
        return column_id_;
    }

    [[nodiscard]] meta::entry::VectorDistanceMetric metric() const noexcept
    {
        return metric_;
    }

    [[nodiscard]] const binder::bound::BoundExpression & query_vector() const noexcept
    {
        return *query_vector_;
    }

    [[nodiscard]] const binder::bound::BoundExpression * query_vector_ptr() const noexcept
    {
        return query_vector_.get();
    }

    [[nodiscard]] const binder::bound::BoundExpression * predicate() const noexcept
    {
        return predicate_.get();
    }

    [[nodiscard]] std::size_t required_count() const noexcept
    {
        return required_count_;
    }

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

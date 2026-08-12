#include "core/physical_planner/operator/physical_vector_search_operator.hpp"

#include <utility>

namespace litedb::core::physical_planner::op
{

VectorSearchOperator::VectorSearchOperator(
    common::CollectionId collection_id,
    common::VIndexId index_id,
    common::ColumnId column_id,
    catalog::entry::VectorDistanceMetric metric,
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
{}

common::CollectionId VectorSearchOperator::collection_id() const noexcept
{
    return collection_id_;
}

common::VIndexId VectorSearchOperator::index_id() const noexcept
{
    return index_id_;
}

common::ColumnId VectorSearchOperator::column_id() const noexcept
{
    return column_id_;
}

catalog::entry::VectorDistanceMetric VectorSearchOperator::metric() const noexcept
{
    return metric_;
}

const binder::bound::BoundExpression & VectorSearchOperator::query_vector() const noexcept
{
    return *query_vector_;
}

std::optional<const binder::bound::BoundExpression &>
VectorSearchOperator::predicate() const noexcept
{
    if (predicate_ == nullptr) {
        return std::nullopt;
    }
    return *predicate_;
}

std::size_t VectorSearchOperator::required_count() const noexcept
{
    return required_count_;
}

} // namespace litedb::core::physical_planner::op

#include "core/binder/bound/statement/bound_select_statement.hpp"

#include <utility>

namespace litedb::core::binder::bound
{

BoundSelectStatement::BoundSelectStatement(
    common::CollectionId collection_id,
    std::vector<BoundProjectionItem> projections,
    std::unique_ptr<BoundExpression> where,
    std::vector<BoundOrderByItem> order_by,
    std::optional<std::size_t> limit,
    std::optional<std::size_t> offset
)
    : BoundStatement(BoundStatementKind::Select)
    , collection_id_(collection_id)
    , projections_(std::move(projections))
    , where_(std::move(where))
    , order_by_(std::move(order_by))
    , limit_(limit)
    , offset_(offset)
{}

common::CollectionId BoundSelectStatement::collection_id() const noexcept
{
    return collection_id_;
}

const std::vector<BoundProjectionItem> & BoundSelectStatement::projections() const noexcept
{
    return projections_;
}

std::optional<const BoundExpression &> BoundSelectStatement::where() const noexcept
{
    if (!where_) {
        return std::nullopt;
    }

    return *where_;
}

const std::vector<BoundOrderByItem> & BoundSelectStatement::order_by() const noexcept
{
    return order_by_;
}

std::optional<std::size_t> BoundSelectStatement::limit() const noexcept
{
    return limit_;
}

std::optional<std::size_t> BoundSelectStatement::offset() const noexcept
{
    return offset_;
}

std::vector<BoundProjectionItem> BoundSelectStatement::take_projections() noexcept
{
    return std::move(projections_);
}

std::unique_ptr<BoundExpression> BoundSelectStatement::take_where() noexcept
{
    return std::move(where_);
}

std::vector<BoundOrderByItem> BoundSelectStatement::take_order_by() noexcept
{
    return std::move(order_by_);
}

} // namespace litedb::core::binder::bound

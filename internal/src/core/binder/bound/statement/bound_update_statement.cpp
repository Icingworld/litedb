#include "core/binder/bound/statement/bound_update_statement.hpp"

#include <utility>

namespace litedb::core::binder::bound
{

BoundUpdateStatement::BoundUpdateStatement(
    common::CollectionId collection_id,
    std::vector<BoundAssignment> assignments,
    std::unique_ptr<BoundExpression> where
)
    : BoundStatement(BoundStatementKind::Update)
    , collection_id_(collection_id)
    , assignments_(std::move(assignments))
    , where_(std::move(where))
{}

common::CollectionId BoundUpdateStatement::collection_id() const noexcept
{
    return collection_id_;
}

const std::vector<BoundAssignment> & BoundUpdateStatement::assignments() const noexcept
{
    return assignments_;
}

std::optional<const BoundExpression &> BoundUpdateStatement::where() const noexcept
{
    if (!where_) {
        return std::nullopt;
    }

    return *where_;
}

std::vector<BoundAssignment> BoundUpdateStatement::take_assignments() noexcept
{
    return std::move(assignments_);
}

std::unique_ptr<BoundExpression> BoundUpdateStatement::take_where() noexcept
{
    return std::move(where_);
}

} // namespace litedb::core::binder::bound

#include "core/binder/bound/statement/bound_delete_statement.hpp"

#include <utility>

namespace litedb::core::binder::bound
{

BoundDeleteStatement::BoundDeleteStatement(
    common::CollectionId collection_id,
    std::unique_ptr<BoundExpression> where
)
    : BoundStatement(BoundStatementKind::Delete)
    , collection_id_(collection_id)
    , where_(std::move(where))
{
}

common::CollectionId BoundDeleteStatement::collection_id() const noexcept
{
    return collection_id_;
}

const BoundExpression * BoundDeleteStatement::where() const noexcept
{
    return where_.get();
}

std::unique_ptr<BoundExpression> BoundDeleteStatement::take_where() noexcept
{
    return std::move(where_);
}

} // namespace litedb::core::binder::bound

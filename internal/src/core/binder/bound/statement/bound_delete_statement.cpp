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
{}

common::CollectionId BoundDeleteStatement::collection_id() const noexcept
{
    return collection_id_;
}

std::optional<const BoundExpression &> BoundDeleteStatement::where() const noexcept
{
    if (!where_) {
        return std::nullopt;
    }

    return *where_;
}

std::unique_ptr<BoundExpression> BoundDeleteStatement::take_where() noexcept
{
    return std::exchange(where_, nullptr);
}

} // namespace litedb::core::binder::bound

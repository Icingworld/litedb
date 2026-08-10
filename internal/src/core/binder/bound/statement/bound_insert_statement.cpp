#include "core/binder/bound/statement/bound_insert_statement.hpp"

#include <utility>

namespace litedb::core::binder::bound
{

BoundInsertStatement::BoundInsertStatement(
    common::CollectionId collection_id,
    std::vector<std::unique_ptr<BoundExpression>> values
)
    : BoundStatement(BoundStatementKind::Insert)
    , collection_id_(collection_id)
    , values_(std::move(values))
{}

common::CollectionId BoundInsertStatement::collection_id() const noexcept
{
    return collection_id_;
}

const std::vector<std::unique_ptr<BoundExpression>> & BoundInsertStatement::values() const noexcept
{
    return values_;
}

std::vector<std::unique_ptr<BoundExpression>> BoundInsertStatement::take_values() noexcept
{
    return std::exchange(values_, {});
}

} // namespace litedb::core::binder::bound

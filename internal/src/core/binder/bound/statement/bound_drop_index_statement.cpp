#include "core/binder/bound/statement/bound_drop_index_statement.hpp"

namespace litedb::core::binder::bound
{

BoundDropIndexStatement::BoundDropIndexStatement(
    std::optional<common::IndexId> index_id
) noexcept
    : BoundStatement(BoundStatementKind::DropIndex)
    , index_id_(index_id)
{
}

std::optional<common::IndexId>
BoundDropIndexStatement::index_id() const noexcept
{
    return index_id_;
}

} // namespace litedb::core::binder::bound

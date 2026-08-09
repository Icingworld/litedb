#include "core/binder/bound/statement/bound_drop_database_statement.hpp"

namespace litedb::core::binder::bound
{

BoundDropDatabaseStatement::BoundDropDatabaseStatement(
    std::optional<common::DatabaseId> database_id
) noexcept
    : BoundStatement(BoundStatementKind::DropDatabase)
    , database_id_(database_id)
{}

std::optional<common::DatabaseId> BoundDropDatabaseStatement::database_id() const noexcept
{
    return database_id_;
}

} // namespace litedb::core::binder::bound

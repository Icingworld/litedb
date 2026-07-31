#include "core/binder/bound/statement/bound_create_database_statement.hpp"

#include <utility>

namespace litedb::core::binder::bound
{

BoundCreateDatabaseStatement::BoundCreateDatabaseStatement(
    std::optional<std::string> database_name
) noexcept
    : BoundStatement(BoundStatementKind::CreateDatabase)
    , database_name_(std::move(database_name))
{
}

const std::optional<std::string> &
BoundCreateDatabaseStatement::database_name() const noexcept
{
    return database_name_;
}

} // namespace litedb::core::binder::bound

#include "core/binder/bound/statement/bound_drop_database_statement.hpp"

#include <utility>

namespace litedb::core::binder::bound
{

BoundDropDatabaseStatement::BoundDropDatabaseStatement(
    std::optional<common::DatabaseId> database_id,
    std::string database_name,
    bool if_exists,
    parser::ast::AstNodeLocation location
)
    : BoundStatement(BoundStatementKind::DropDatabase, location),
      database_id_(database_id),
      database_name_(std::move(database_name)),
      if_exists_(if_exists)
{
}

std::optional<common::DatabaseId> BoundDropDatabaseStatement::database_id() const noexcept { return database_id_; }
const std::string & BoundDropDatabaseStatement::database_name() const noexcept { return database_name_; }
bool BoundDropDatabaseStatement::if_exists() const noexcept { return if_exists_; }

} // namespace litedb::core::binder::bound

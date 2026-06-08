#include "core/binder/bound/statement/bound_create_database_statement.hpp"

#include <utility>

namespace litedb::core::binder::bound
{

BoundCreateDatabaseStatement::BoundCreateDatabaseStatement(
    std::string database_name,
    bool if_not_exists,
    parser::ast::AstNodeLocation location
)
    : BoundStatement(BoundStatementKind::CreateDatabase, location),
      database_name_(std::move(database_name)),
      if_not_exists_(if_not_exists)
{
}

const std::string & BoundCreateDatabaseStatement::database_name() const noexcept { return database_name_; }
bool BoundCreateDatabaseStatement::if_not_exists() const noexcept { return if_not_exists_; }

} // namespace litedb::core::binder::bound

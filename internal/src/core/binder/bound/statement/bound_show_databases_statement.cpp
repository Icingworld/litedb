#include "core/binder/bound/statement/bound_show_databases_statement.hpp"

namespace litedb::core::binder::bound
{

BoundShowDatabasesStatement::BoundShowDatabasesStatement() noexcept
    : BoundStatement(BoundStatementKind::ShowDatabases)
{}

} // namespace litedb::core::binder::bound

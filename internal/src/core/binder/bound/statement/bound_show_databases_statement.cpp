#include "core/binder/bound/statement/bound_show_databases_statement.hpp"

namespace litedb::core::binder::bound
{

BoundShowDatabasesStatement::BoundShowDatabasesStatement(parser::ast::AstNodeLocation location)
    : BoundStatement(BoundStatementKind::ShowDatabases, location)
{
}

} // namespace litedb::core::binder::bound

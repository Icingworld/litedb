#include "core/binder/bound/statement/bound_show_databases_statement.hpp"

namespace litedb::core::binder::bound
{

BoundShowDatabasesStatement::BoundShowDatabasesStatement(parser::ast::AstNodeLocation location)
    : BoundStatement(BoundStatementKind::ShowDatabases, location)
{
}

void BoundShowDatabasesStatement::accept(BoundStatementVisitor & visitor) const
{
    visitor.visit(*this);
}

} // namespace litedb::core::binder::bound

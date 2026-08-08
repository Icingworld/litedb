#include "core/parser/ast/statement/show_databases_statement.hpp"

namespace litedb::core::parser::ast
{

ShowDatabasesStatement::ShowDatabasesStatement(AstNodeLocation location) noexcept
    : StatementNode(location)
{}

AstNodeKind ShowDatabasesStatement::kind() const noexcept
{
    return AstNodeKind::ShowDatabases;
}

} // namespace litedb::core::parser::ast

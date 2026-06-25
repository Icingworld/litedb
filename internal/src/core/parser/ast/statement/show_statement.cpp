#include "core/parser/ast/statement/show_statement.hpp"

namespace litedb::core::parser::ast
{

ShowStatement::ShowStatement(SchemaObjectType object_type, AstNodeLocation location) noexcept
    : StatementNode(location)
    , object_type_(object_type)
{
}

AstNodeKind ShowStatement::kind() const noexcept
{
    return AstNodeKind::Show;
}

void ShowStatement::accept(AstNodeVisitor & visitor) const
{
    visitor.visit(*this);
}

SchemaObjectType ShowStatement::object_type() const noexcept
{
    return object_type_;
}

} // namespace litedb::core::parser::ast

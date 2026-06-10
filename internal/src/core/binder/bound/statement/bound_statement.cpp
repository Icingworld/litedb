#include "core/binder/bound/statement/bound_statement.hpp"

namespace litedb::core::binder::bound
{

BoundStatement::BoundStatement(BoundStatementKind kind, parser::ast::AstNodeLocation location) noexcept
    : kind_(kind),
      location_(location)
{
}

BoundStatementKind BoundStatement::kind() const noexcept
{
    return kind_;
}

parser::ast::AstNodeLocation BoundStatement::location() const noexcept
{
    return location_;
}

} // namespace litedb::core::binder::bound

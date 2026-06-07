#include "core/binder/bound/statement/bound_statement.hpp"

namespace litedb::core::binder::bound
{

BoundStatement::BoundStatement(BoundStatementKind kind) noexcept
    : kind_(kind)
{
}

BoundStatementKind BoundStatement::kind() const noexcept
{
    return kind_;
}

} // namespace litedb::core::binder::bound

#include "core/binder/bound/statement/bound_use_statement.hpp"

#include <utility>

namespace litedb::core::binder::bound
{

BoundUseStatement::BoundUseStatement(
    common::DatabaseId database_id,
    std::string database_name,
    parser::ast::AstNodeLocation location
)
    : BoundStatement(BoundStatementKind::Use, location)
    , database_id_(database_id)
    , database_name_(std::move(database_name))
{
}

common::DatabaseId BoundUseStatement::database_id() const noexcept
{
    return database_id_;
}

const std::string & BoundUseStatement::database_name() const noexcept
{
    return database_name_;
}

void BoundUseStatement::accept(BoundStatementVisitor & visitor) const
{
    visitor.visit(*this);
}

} // namespace litedb::core::binder::bound

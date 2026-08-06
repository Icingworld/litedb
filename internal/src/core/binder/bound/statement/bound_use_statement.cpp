#include "core/binder/bound/statement/bound_use_statement.hpp"

namespace litedb::core::binder::bound
{

BoundUseStatement::BoundUseStatement(
    common::DatabaseId database_id
) noexcept
    : BoundStatement(BoundStatementKind::Use)
    , database_id_(database_id)
{
}

common::DatabaseId BoundUseStatement::database_id() const noexcept
{
    return database_id_;
}

} // namespace litedb::core::binder::bound

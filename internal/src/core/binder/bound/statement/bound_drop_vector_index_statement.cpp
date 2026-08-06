#include "core/binder/bound/statement/bound_drop_vector_index_statement.hpp"

namespace litedb::core::binder::bound
{

BoundDropVectorIndexStatement::BoundDropVectorIndexStatement(
    std::optional<common::VIndexId> vector_index_id
) noexcept
    : BoundStatement(BoundStatementKind::DropVectorIndex)
    , vector_index_id_(vector_index_id)
{
}

std::optional<common::VIndexId>
BoundDropVectorIndexStatement::vector_index_id() const noexcept
{
    return vector_index_id_;
}

} // namespace litedb::core::binder::bound

#include "core/binder/bound/statement/bound_create_index_statement.hpp"

#include <utility>

namespace litedb::core::binder::bound
{

BoundCreateIndexStatement::BoundCreateIndexStatement(
    common::ColumnId column_id,
    std::optional<std::string> index_name,
    meta::entry::IndexKind index_kind,
    bool unique
)
    : BoundStatement(BoundStatementKind::CreateIndex)
    , column_id_(column_id)
    , index_name_(std::move(index_name))
    , index_kind_(index_kind)
    , unique_(unique)
{}

const std::optional<std::string> & BoundCreateIndexStatement::index_name() const noexcept
{
    return index_name_;
}

common::ColumnId BoundCreateIndexStatement::column_id() const noexcept
{
    return column_id_;
}

meta::entry::IndexKind BoundCreateIndexStatement::index_kind() const noexcept
{
    return index_kind_;
}

bool BoundCreateIndexStatement::unique() const noexcept
{
    return unique_;
}

} // namespace litedb::core::binder::bound

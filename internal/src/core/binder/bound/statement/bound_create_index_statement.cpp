#include "core/binder/bound/statement/bound_create_index_statement.hpp"

#include <utility>

namespace litedb::core::binder::bound
{

BoundCreateIndexStatement::BoundCreateIndexStatement(
    common::DatabaseId database_id,
    common::CollectionId collection_id,
    std::string collection_name,
    common::ColumnId column_id,
    std::string column_name,
    std::string index_name,
    meta::entry::IndexKind index_kind,
    bool unique,
    bool if_not_exists,
    parser::ast::AstNodeLocation location
)
    : BoundStatement(BoundStatementKind::CreateIndex, location)
    , database_id_(database_id)
    , collection_id_(collection_id)
    , collection_name_(std::move(collection_name))
    , column_id_(column_id)
    , column_name_(std::move(column_name))
    , index_name_(std::move(index_name))
    , index_kind_(index_kind)
    , unique_(unique)
    , if_not_exists_(if_not_exists)
{
}

common::DatabaseId BoundCreateIndexStatement::database_id() const noexcept
{
    return database_id_;
}

common::CollectionId BoundCreateIndexStatement::collection_id() const noexcept
{
    return collection_id_;
}

const std::string & BoundCreateIndexStatement::collection_name() const noexcept
{
    return collection_name_;
}

common::ColumnId BoundCreateIndexStatement::column_id() const noexcept
{
    return column_id_;
}

const std::string & BoundCreateIndexStatement::column_name() const noexcept
{
    return column_name_;
}

const std::string & BoundCreateIndexStatement::index_name() const noexcept
{
    return index_name_;
}

meta::entry::IndexKind BoundCreateIndexStatement::index_kind() const noexcept
{
    return index_kind_;
}

bool BoundCreateIndexStatement::unique() const noexcept
{
    return unique_;
}

bool BoundCreateIndexStatement::if_not_exists() const noexcept
{
    return if_not_exists_;
}

void BoundCreateIndexStatement::accept(BoundStatementVisitor & visitor) const
{
    visitor.visit(*this);
}

} // namespace litedb::core::binder::bound

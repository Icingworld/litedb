#include "core/binder/bound/statement/bound_create_collection_statement.hpp"

#include <utility>

namespace litedb::core::binder::bound
{

BoundCreateCollectionStatement::BoundCreateCollectionStatement(
    common::DatabaseId database_id,
    std::optional<std::string> collection_name,
    std::vector<meta::ColumnDefinition> columns,
    std::optional<std::string> comment
)
    : BoundStatement(BoundStatementKind::CreateCollection)
    , database_id_(database_id)
    , collection_name_(std::move(collection_name))
    , columns_(std::move(columns))
    , comment_(std::move(comment))
{}

common::DatabaseId BoundCreateCollectionStatement::database_id() const noexcept
{
    return database_id_;
}

const std::optional<std::string> & BoundCreateCollectionStatement::collection_name() const noexcept
{
    return collection_name_;
}

const std::vector<meta::ColumnDefinition> & BoundCreateCollectionStatement::columns() const noexcept
{
    return columns_;
}

const std::optional<std::string> & BoundCreateCollectionStatement::comment() const noexcept
{
    return comment_;
}

} // namespace litedb::core::binder::bound

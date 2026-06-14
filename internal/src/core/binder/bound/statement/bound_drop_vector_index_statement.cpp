#include "core/binder/bound/statement/bound_drop_vector_index_statement.hpp"

#include <utility>

namespace litedb::core::binder::bound
{

BoundDropVectorIndexStatement::BoundDropVectorIndexStatement(
    common::DatabaseId database_id,
    common::CollectionId collection_id,
    std::string collection_name,
    std::string index_name,
    bool if_exists,
    parser::ast::AstNodeLocation location
)
    : BoundStatement(BoundStatementKind::DropVectorIndex, location)
    , database_id_(database_id)
    , collection_id_(collection_id)
    , collection_name_(std::move(collection_name))
    , index_name_(std::move(index_name))
    , if_exists_(if_exists)
{
}

common::DatabaseId BoundDropVectorIndexStatement::database_id() const noexcept { return database_id_; }

common::CollectionId BoundDropVectorIndexStatement::collection_id() const noexcept { return collection_id_; }

const std::string & BoundDropVectorIndexStatement::collection_name() const noexcept { return collection_name_; }

const std::string & BoundDropVectorIndexStatement::index_name() const noexcept { return index_name_; }

bool BoundDropVectorIndexStatement::if_exists() const noexcept { return if_exists_; }

} // namespace litedb::core::binder::bound

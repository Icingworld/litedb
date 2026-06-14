#include "core/binder/bound/statement/bound_create_vector_index_statement.hpp"

#include <utility>

namespace litedb::core::binder::bound
{

BoundCreateVectorIndexStatement::BoundCreateVectorIndexStatement(
    common::DatabaseId database_id,
    common::CollectionId collection_id,
    std::string collection_name,
    common::ColumnId column_id,
    std::string column_name,
    std::string index_name,
    catalog::CatalogVectorIndexKind index_kind,
    catalog::CatalogVectorDistanceMetric metric,
    std::size_t max_neighbors,
    std::size_t ef_construction,
    std::size_t ef_search_default,
    std::size_t random_seed,
    bool if_not_exists,
    parser::ast::AstNodeLocation location
)
    : BoundStatement(BoundStatementKind::CreateVectorIndex, location)
    , database_id_(database_id)
    , collection_id_(collection_id)
    , collection_name_(std::move(collection_name))
    , column_id_(column_id)
    , column_name_(std::move(column_name))
    , index_name_(std::move(index_name))
    , index_kind_(index_kind)
    , metric_(metric)
    , max_neighbors_(max_neighbors)
    , ef_construction_(ef_construction)
    , ef_search_default_(ef_search_default)
    , random_seed_(random_seed)
    , if_not_exists_(if_not_exists)
{
}

common::DatabaseId BoundCreateVectorIndexStatement::database_id() const noexcept { return database_id_; }

common::CollectionId BoundCreateVectorIndexStatement::collection_id() const noexcept { return collection_id_; }

const std::string & BoundCreateVectorIndexStatement::collection_name() const noexcept { return collection_name_; }

common::ColumnId BoundCreateVectorIndexStatement::column_id() const noexcept { return column_id_; }

const std::string & BoundCreateVectorIndexStatement::column_name() const noexcept { return column_name_; }

const std::string & BoundCreateVectorIndexStatement::index_name() const noexcept { return index_name_; }

catalog::CatalogVectorIndexKind BoundCreateVectorIndexStatement::index_kind() const noexcept { return index_kind_; }

catalog::CatalogVectorDistanceMetric BoundCreateVectorIndexStatement::metric() const noexcept { return metric_; }

std::size_t BoundCreateVectorIndexStatement::max_neighbors() const noexcept { return max_neighbors_; }

std::size_t BoundCreateVectorIndexStatement::ef_construction() const noexcept { return ef_construction_; }

std::size_t BoundCreateVectorIndexStatement::ef_search_default() const noexcept { return ef_search_default_; }

std::size_t BoundCreateVectorIndexStatement::random_seed() const noexcept { return random_seed_; }

bool BoundCreateVectorIndexStatement::if_not_exists() const noexcept { return if_not_exists_; }

} // namespace litedb::core::binder::bound

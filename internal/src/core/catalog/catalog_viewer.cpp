#include "core/catalog/catalog_viewer.hpp"

namespace litedb::core::catalog
{

CatalogViewer::CatalogViewer(const CatalogState & state) noexcept
    : state_(state)
{}

std::optional<const entry::DatabaseEntry &> CatalogViewer::find_database(
    std::string_view name
) const
{
    return state_.find_database(name);
}

std::optional<const entry::DatabaseEntry &> CatalogViewer::find_database(
    common::DatabaseId id
) const
{
    return state_.find_database(id);
}

std::optional<const entry::CollectionEntry &>
CatalogViewer::find_collection(common::DatabaseId database_id, std::string_view name) const
{
    return state_.find_collection(database_id, name);
}

std::optional<const entry::CollectionEntry &> CatalogViewer::find_collection(
    common::CollectionId id
) const
{
    return state_.find_collection(id);
}

std::optional<const entry::ColumnEntry &>
CatalogViewer::find_column(common::CollectionId collection_id, std::string_view name) const
{
    return state_.find_column(collection_id, name);
}

std::optional<const entry::ColumnEntry &> CatalogViewer::find_column(common::ColumnId id) const
{
    return state_.find_column(id);
}

std::optional<const entry::IndexEntry &>
CatalogViewer::find_index(common::CollectionId collection_id, std::string_view name) const
{
    return state_.find_index(collection_id, name);
}

std::optional<const entry::IndexEntry &> CatalogViewer::find_index(common::IndexId id) const
{
    return state_.find_index(id);
}

std::optional<const entry::VectorIndexEntry &>
CatalogViewer::find_vector_index(common::CollectionId collection_id, std::string_view name) const
{
    return state_.find_vector_index(collection_id, name);
}

std::optional<const entry::VectorIndexEntry &> CatalogViewer::find_vector_index(
    common::VIndexId id
) const
{
    return state_.find_vector_index(id);
}

std::vector<std::reference_wrapper<const entry::DatabaseEntry>>
CatalogViewer::list_databases() const
{
    return state_.list_databases();
}

std::vector<std::reference_wrapper<const entry::CollectionEntry>> CatalogViewer::list_collections(
    common::DatabaseId database_id
) const
{
    return state_.list_collections(database_id);
}

std::vector<std::reference_wrapper<const entry::ColumnEntry>> CatalogViewer::list_columns(
    common::CollectionId collection_id
) const
{
    return state_.list_columns(collection_id);
}

std::vector<std::reference_wrapper<const entry::IndexEntry>> CatalogViewer::list_indexes(
    common::CollectionId collection_id
) const
{
    return state_.list_indexes(collection_id);
}

std::vector<std::reference_wrapper<const entry::VectorIndexEntry>>
CatalogViewer::list_vector_indexes(common::CollectionId collection_id) const
{
    return state_.list_vector_indexes(collection_id);
}

CatalogSnapshot CatalogViewer::snapshot() const
{
    return state_.snapshot();
}

} // namespace litedb::core::catalog

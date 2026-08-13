#include "core/catalog/catalog_editor.hpp"

#include <utility>

namespace litedb::core::catalog
{

std::expected<CatalogEditor, CatalogError> CatalogEditor::from(CatalogViewer source)
{
    return from(source.snapshot());
}

std::expected<CatalogEditor, CatalogError> CatalogEditor::from(const CatalogSnapshot & source)
{
    auto state = build_catalog_state(source);
    if (!state) [[unlikely]] {
        return std::unexpected(std::move(state.error()));
    }
    CatalogEditor editor;
    editor.state_ = std::move(*state);
    return editor;
}

CatalogViewer CatalogEditor::view() const noexcept
{
    return CatalogViewer {state_};
}

CatalogSnapshot CatalogEditor::snapshot() const
{
    return state_.snapshot();
}

std::expected<common::DatabaseId, CatalogError> CatalogEditor::create_database(
    const CreateDatabaseRequest & request
)
{
    return state_.create_database(request);
}

std::expected<void, CatalogError> CatalogEditor::drop_database(const DropDatabaseRequest & request)
{
    return state_.drop_database(request);
}

std::expected<common::CollectionId, CatalogError> CatalogEditor::create_collection(
    const CreateCollectionRequest & request
)
{
    return state_.create_collection(request);
}

std::expected<void, CatalogError> CatalogEditor::drop_collection(
    const DropCollectionRequest & request
)
{
    return state_.drop_collection(request);
}

std::expected<common::IndexId, CatalogError> CatalogEditor::create_index(
    const CreateIndexRequest & request
)
{
    return state_.create_index(request);
}

std::expected<void, CatalogError> CatalogEditor::drop_index(const DropIndexRequest & request)
{
    return state_.drop_index(request);
}

std::expected<common::VIndexId, CatalogError> CatalogEditor::create_vector_index(
    const CreateVectorIndexRequest & request
)
{
    return state_.create_vector_index(request);
}

std::expected<void, CatalogError> CatalogEditor::drop_vector_index(
    const DropVectorIndexRequest & request
)
{
    return state_.drop_vector_index(request);
}

} // namespace litedb::core::catalog

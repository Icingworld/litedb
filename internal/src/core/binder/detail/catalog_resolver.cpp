#include "core/binder/detail/catalog_resolver.hpp"

#include <utility>

#include "core/binder/binder_context.hpp"
#include "core/binder/binder_helper.hpp"

namespace litedb::core::binder::detail
{

CatalogResolver::CatalogResolver(const BinderContext & context)
    : context_(context)
{}

std::expected<common::DatabaseId, BinderError> CatalogResolver::require_database() const
{
    if (!context_.session().current_database_id.has_value()) [[unlikely]] {
        return std::unexpected(
            make_binder_error(BinderErrorCode::DatabaseNotSelected, "No database selected")
        );
    }

    if (!context_.catalog().find_database(context_.session().current_database_id.value()))
        [[unlikely]] {
        return std::unexpected(
            make_binder_error(BinderErrorCode::DatabaseNotFound, "Current database not found")
        );
    }

    return context_.session().current_database_id.value();
}

std::expected<BindingCollection, BinderError> CatalogResolver::resolve_collection(
    const std::string & collection_name
) const
{
    auto database_id = require_database();
    if (!database_id.has_value()) [[unlikely]] {
        return std::unexpected(std::move(database_id.error()));
    }

    const auto collection = context_.catalog().find_collection(*database_id, collection_name);
    if (!collection) [[unlikely]] {
        return std::unexpected(make_binder_error(
            BinderErrorCode::CollectionNotFound,
            "Collection not found: " + collection_name
        ));
    }

    return BindingCollection {
        .database_id = *database_id,
        .collection = &*collection,
    };
}

} // namespace litedb::core::binder::detail

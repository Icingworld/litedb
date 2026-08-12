#include "core/catalog/catalog_error.hpp"

#include <utility>

namespace litedb::core::catalog
{

CatalogError make_error(CatalogErrorCode code, std::string message, CatalogErrorContext context)
{
    return CatalogError {code, message, std::move(context)};
}

} // namespace litedb::core::catalog

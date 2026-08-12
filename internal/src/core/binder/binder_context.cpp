#include "core/binder/binder_context.hpp"

namespace litedb::core::binder
{

BinderContext::BinderContext(
    catalog::CatalogViewer catalog,
    const SessionContext & session,
    const function::FunctionCatalog & functions
) noexcept
    : catalog_(catalog)
    , session_(session)
    , functions_(functions)
{}

const catalog::CatalogViewer & BinderContext::catalog() const noexcept
{
    return catalog_;
}

const SessionContext & BinderContext::session() const noexcept
{
    return session_;
}

const function::FunctionCatalog & BinderContext::functions() const noexcept
{
    return functions_;
}

} // namespace litedb::core::binder

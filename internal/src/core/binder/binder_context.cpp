#include "core/binder/binder_context.hpp"

namespace litedb::core::binder
{

BinderContext::BinderContext(
    meta::CatalogView meta,
    const SessionContext & session,
    const function::FunctionCatalog & functions
) noexcept
    : meta_(meta)
    , session_(session)
    , functions_(functions)
{}

const meta::CatalogView & BinderContext::meta() const noexcept
{
    return meta_;
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

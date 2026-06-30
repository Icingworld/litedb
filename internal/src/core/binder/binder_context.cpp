#include "core/binder/binder_context.hpp"

namespace litedb::core::binder
{

BinderContext::BinderContext(const catalog::CatalogReader & catalog, const SessionContext & session) noexcept
    : catalog_(catalog)
    , session_(session)
{
}

const catalog::CatalogReader & BinderContext::catalog() const noexcept
{
    return catalog_;
}

const SessionContext & BinderContext::session() const noexcept
{
    return session_;
}

} // namespace litedb::core::binder

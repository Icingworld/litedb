#include "core/binder/binder_context.hpp"

namespace litedb::core::binder
{

BinderContext::BinderContext(meta::CatalogView meta, const SessionContext & session) noexcept
    : meta_(meta)
    , session_(session)
{
}

const meta::CatalogView & BinderContext::meta() const noexcept
{
    return meta_;
}

const SessionContext & BinderContext::session() const noexcept
{
    return session_;
}

} // namespace litedb::core::binder

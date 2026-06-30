#include "core/binder/binder.hpp"

#include "core/binder/worker/binder_worker.hpp"

namespace litedb::core::binder
{

Binder::Binder(const catalog::CatalogReader & catalog, const SessionContext & session) noexcept
    : catalog_(catalog)
    , session_(session)
{
}

std::expected<std::unique_ptr<bound::BoundStatement>, BinderError> Binder::bind(
    const parser::ast::StatementNode & statement
) const
{
    return BinderWorker(catalog_, session_).bind_statement(statement);
}

} // namespace litedb::core::binder

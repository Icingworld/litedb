#include "core/binder/binder.hpp"

#include "core/binder/worker/binder_worker.hpp"

namespace litedb::core::binder
{

Binder::Binder(const BinderContext & context) noexcept
    : context_(context)
{
}

std::expected<std::unique_ptr<bound::BoundStatement>, BinderError> Binder::bind(
    const parser::ast::StatementNode & statement
) const
{
    return BinderWorker(context_).bind_statement(statement);
}

} // namespace litedb::core::binder

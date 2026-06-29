#include "core/binder/worker/binder_worker.hpp"

#include "core/parser/ast/statement/statement_node.hpp"

namespace litedb::core::binder
{

BinderWorker::BinderWorker(const catalog::CatalogReader & catalog) noexcept
    : catalog_(catalog)
{
}

std::expected<std::unique_ptr<bound::BoundStatement>, BinderError> BinderWorker::bind(const parser::ast::StatementNode & node) const
{
    return std::unexpected(BinderError(BinderErrorCode::UnsupportedStatement, node.location(), "Unimplemented"));
}

} // namespace litedb::core::binder

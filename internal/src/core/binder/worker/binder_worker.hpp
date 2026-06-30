#pragma once

#include <expected>
#include <memory>

#include "core/binder/binder_context.hpp"
#include "core/binder/binder_error.hpp"

namespace litedb::core::parser::ast
{

class StatementNode;

} // namespace litedb::core::parser::ast

namespace litedb::core::binder::bound
{

class BoundStatement;

} // namespace litedb::core::binder::bound

namespace litedb::core::binder
{

/**
 * @brief 绑定工作器
 */
class BinderWorker
{
public:
    BinderWorker(const catalog::CatalogReader & catalog, const SessionContext & session);

public:
    /**
     * @brief 绑定语句
     * @param statement 语句
     * @return 绑定后的语句
     */
    [[nodiscard]]
    std::expected<std::unique_ptr<bound::BoundStatement>, BinderError> bind_statement(
        const parser::ast::StatementNode & statement
    );

private:
    BinderContext context_;         ///< 绑定上下文
};

} // namespace litedb::core::binder

#pragma once

#include <expected>
#include <memory>

#include "core/binder/binder_error.hpp"
#include "core/binder/bound/statement/bound_statement.hpp"
#include "core/binder/session_context.hpp"
#include "core/catalog/catalog_reader.hpp"

namespace litedb::core::parser::ast
{

class StatementNode;

} // namespace litedb::core::parser::ast

namespace litedb::core::binder
{

/**
 * @brief 绑定器
 * @details 用于将 SQL 语句解析为绑定后的语句节点
 */
class Binder
{
public:
    /**
     * @brief 构造绑定器
     * @param catalog 数据库读取器
     * @param session 会话上下文
     */
    Binder(const catalog::CatalogReader & catalog, const SessionContext & session) noexcept;

public:
    /**
     * @brief 绑定 SQL 语句
     * @param statement SQL 语句
     * @return 绑定后的语句节点
     */
    [[nodiscard]]
    std::expected<std::unique_ptr<bound::BoundStatement>, BinderError> bind(
        const parser::ast::StatementNode & statement
    ) const;

private:
    const catalog::CatalogReader & catalog_;    ///< 数据库读取器
    const SessionContext & session_;            ///< 会话上下文
};

} // namespace litedb::core::binder

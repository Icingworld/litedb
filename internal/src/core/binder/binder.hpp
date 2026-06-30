#pragma once

#include <expected>
#include <memory>

#include "core/binder/binder_error.hpp"
#include "core/binder/bound/statement/bound_statement.hpp"
#include "core/binder/binder_context.hpp"

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
     * @param context 绑定上下文
     */
    Binder(const BinderContext & context) noexcept;

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
    const BinderContext & context_;            ///< 绑定上下文
};

} // namespace litedb::core::binder

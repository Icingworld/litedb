#pragma once

#include <expected>
#include <memory>

#include "core/binder/binder_context.hpp"
#include "core/binder/binder_error.hpp"
#include "core/binder/bound/statement/bound_statement.hpp"

namespace litedb::core::parser::ast
{

class StatementNode;

} // namespace litedb::core::parser::ast

namespace litedb::core::binder
{

// 绑定器
class Binder
{
public:
    explicit Binder(const BinderContext & context) noexcept;

public:
    // 绑定 SQL 语句
    [[nodiscard]]
    std::expected<std::unique_ptr<bound::BoundStatement>, BinderError> bind(
        const parser::ast::StatementNode & statement
    ) const;

private:
    const BinderContext & context_;
};

} // namespace litedb::core::binder

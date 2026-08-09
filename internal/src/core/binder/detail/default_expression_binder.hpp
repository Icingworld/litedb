#pragma once

#include <expected>
#include <memory>

#include "core/binder/binder_error.hpp"
#include "core/schema/default_expression.hpp"

namespace litedb::core::parser::ast
{

class ExpressionNode;

} // namespace litedb::core::parser::ast

namespace litedb::core::binder::bound
{

class BoundExpression;

} // namespace litedb::core::binder::bound

namespace litedb::core::binder::detail
{

// 绑定默认表达式
[[nodiscard]]
std::expected<std::unique_ptr<bound::BoundExpression>, BinderError> bind_default_expression(
    const schema::DefaultExpression & expression
);

// 快照默认表达式
[[nodiscard]]
std::expected<schema::DefaultExpression, BinderError> snapshot_default_expression(
    const parser::ast::ExpressionNode & expression
);

} // namespace litedb::core::binder::detail

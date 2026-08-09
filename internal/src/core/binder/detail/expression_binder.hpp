#pragma once

#include <expected>
#include <memory>

#include "core/binder/binder_error.hpp"
#include "core/binder/detail/catalog_resolver.hpp"
#include "core/parser/ast/dispatcher/expression_dispatcher.hpp"

namespace litedb::core::binder
{

class BinderContext;

} // namespace litedb::core::binder

namespace litedb::core::binder::bound
{

class BoundExpression;

} // namespace litedb::core::binder::bound

namespace litedb::core::binder::detail
{

// 表达式绑定器
class ExpressionBinder final
    : private parser::ast::ConstAstExpressionDispatcher<
          ExpressionBinder,
          std::expected<std::unique_ptr<bound::BoundExpression>, BinderError>>
{
    friend class parser::ast::AstExpressionDispatcher<
        ExpressionBinder,
        std::expected<std::unique_ptr<bound::BoundExpression>, BinderError>,
        true>;

public:
    ExpressionBinder(const BinderContext & context, const BindingCollection & collection);

public:
    // 绑定表达式
    [[nodiscard]]
    std::expected<std::unique_ptr<bound::BoundExpression>, BinderError> bind(
        const parser::ast::ExpressionNode & expression
    );

private:
    // 访问通配符表达式
    [[nodiscard]]
    std::expected<std::unique_ptr<bound::BoundExpression>, BinderError> visit_wildcard_expression(
        const parser::ast::WildcardExpression & expression
    );

    // 访问字面量表达式
    [[nodiscard]]
    std::expected<std::unique_ptr<bound::BoundExpression>, BinderError> visit_literal_expression(
        const parser::ast::LiteralExpression & expression
    );

    // 访问函数调用表达式
    [[nodiscard]]
    std::expected<std::unique_ptr<bound::BoundExpression>, BinderError>
    visit_function_call_expression(const parser::ast::FunctionCallExpression & expression);

    // 访问列引用表达式
    [[nodiscard]]
    std::expected<std::unique_ptr<bound::BoundExpression>, BinderError>
    visit_column_reference_expression(const parser::ast::ColumnReferenceExpression & expression);

    // 访问向量表达式
    [[nodiscard]]
    std::expected<std::unique_ptr<bound::BoundExpression>, BinderError> visit_vector_expression(
        const parser::ast::VectorExpression & expression
    );

    // 访问二元表达式
    [[nodiscard]]
    std::expected<std::unique_ptr<bound::BoundExpression>, BinderError> visit_binary_expression(
        const parser::ast::BinaryExpression & expression
    );

    // 访问一元表达式
    [[nodiscard]]
    std::expected<std::unique_ptr<bound::BoundExpression>, BinderError> visit_unary_expression(
        const parser::ast::UnaryExpression & expression
    );

    // 访问 in 表达式
    [[nodiscard]]
    std::expected<std::unique_ptr<bound::BoundExpression>, BinderError> visit_in_expression(
        const parser::ast::InExpression & expression
    );

    // 访问 between 表达式
    [[nodiscard]]
    std::expected<std::unique_ptr<bound::BoundExpression>, BinderError> visit_between_expression(
        const parser::ast::BetweenExpression & expression
    );

    // 访问 like 表达式
    [[nodiscard]]
    std::expected<std::unique_ptr<bound::BoundExpression>, BinderError> visit_like_expression(
        const parser::ast::LikeExpression & expression
    );

    // 访问别名表达式
    [[nodiscard]]
    std::expected<std::unique_ptr<bound::BoundExpression>, BinderError> visit_alias_expression(
        const parser::ast::AliasExpression & expression
    );

private:
    const BinderContext & context_;
    const BindingCollection & collection_;
};

} // namespace litedb::core::binder::detail

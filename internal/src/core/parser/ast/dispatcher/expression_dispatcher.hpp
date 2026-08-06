#pragma once

#include <utility>
#include <type_traits>

#include "core/parser/ast/expression/alias_expression.hpp"
#include "core/parser/ast/expression/between_expression.hpp"
#include "core/parser/ast/expression/binary_expression.hpp"
#include "core/parser/ast/expression/column_reference_expression.hpp"
#include "core/parser/ast/expression/expression_node.hpp"
#include "core/parser/ast/expression/function_call_expression.hpp"
#include "core/parser/ast/expression/identifier_expression.hpp"
#include "core/parser/ast/expression/in_expression.hpp"
#include "core/parser/ast/expression/like_expression.hpp"
#include "core/parser/ast/expression/literal_expression.hpp"
#include "core/parser/ast/expression/unary_expression.hpp"
#include "core/parser/ast/expression/vector_expression.hpp"
#include "core/parser/ast/expression/wildcard_expression.hpp"

namespace litedb::core::parser::ast
{

/**
 * @brief AST 表达式调度器
 * @tparam Derived 派生类
 * @tparam ReturnType 返回类型
 * @tparam IsConst 是否为常量
 */
template <
    typename Derived,
    typename ReturnType,
    bool IsConst
>
class AstExpressionDispatcher
{
protected:
    /**
     * @brief 引用类型
     */
    template <typename T>
    using ReferenceType = std::conditional_t<
        IsConst,
        const T &,
        T &
    >;

protected:
    /**
     * @brief 调度表达式
     * @param expression 表达式
     * @return 返回值
     */
    [[nodiscard]]
    ReturnType dispatch_expression(ReferenceType<ExpressionNode> expression)
    {
        switch (expression.kind()) {
        case AstNodeKind::Identifier:
            return derived().visit_identifier_expression(
                static_cast<ReferenceType<IdentifierExpression>>(expression)
            );
        case AstNodeKind::Wildcard:
            return derived().visit_wildcard_expression(
                static_cast<ReferenceType<WildcardExpression>>(expression)
            );
        case AstNodeKind::Literal:
            return derived().visit_literal_expression(
                static_cast<ReferenceType<LiteralExpression>>(expression)
            );
        case AstNodeKind::FunctionCall:
            return derived().visit_function_call_expression(
                static_cast<ReferenceType<FunctionCallExpression>>(expression)
            );
        case AstNodeKind::ColumnReference:
            return derived().visit_column_reference_expression(
                static_cast<ReferenceType<ColumnReferenceExpression>>(expression)
            );
        case AstNodeKind::Vector:
            return derived().visit_vector_expression(
                static_cast<ReferenceType<VectorExpression>>(expression)
            );
        case AstNodeKind::Binary:
            return derived().visit_binary_expression(
                static_cast<ReferenceType<BinaryExpression>>(expression)
            );
        case AstNodeKind::Unary:
            return derived().visit_unary_expression(
                static_cast<ReferenceType<UnaryExpression>>(expression)
            );
        case AstNodeKind::In:
            return derived().visit_in_expression(
                static_cast<ReferenceType<InExpression>>(expression)
            );
        case AstNodeKind::Between:
            return derived().visit_between_expression(
                static_cast<ReferenceType<BetweenExpression>>(expression)
            );
        case AstNodeKind::Like:
            return derived().visit_like_expression(
                static_cast<ReferenceType<LikeExpression>>(expression)
            );
        case AstNodeKind::Alias:
            return derived().visit_alias_expression(
                static_cast<ReferenceType<AliasExpression>>(expression)
            );
        default:
            std::unreachable();
        }
    }

private:
    /**
     * @brief 获取派生类引用
     * @return 派生类引用
     */
    [[nodiscard]]
    Derived & derived() noexcept
    {
        return static_cast<Derived &>(*this);
    }
};

/**
 * @brief 常量 AST 表达式调度器
 * @tparam Derived 派生类
 * @tparam ReturnType 返回类型
 */
template <typename Derived, typename ReturnType>
using ConstAstExpressionDispatcher = AstExpressionDispatcher<
    Derived,
    ReturnType,
    true
>;

/**
 * @brief 可变 AST 表达式调度器
 * @tparam Derived 派生类
 * @tparam ReturnType 返回类型
 */
template <typename Derived, typename ReturnType>
using MutableAstExpressionDispatcher = AstExpressionDispatcher<
    Derived,
    ReturnType,
    false
>;

} // namespace litedb::core::parser::ast

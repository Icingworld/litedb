#pragma once

#include <utility>

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
 * @tparam ReturnType 返回类型，默认为 void
 */
template <typename Derived, typename ReturnType = void>
class AstExpressionDispatcher
{
protected:
    /**
     * @brief 调度表达式
     * @param expression 表达式
     * @return 返回值
     */
    [[nodiscard]]
    ReturnType dispatch_expression(const ExpressionNode & expression)
    {
        switch (expression.kind()) {
        case AstNodeKind::Identifier:
            return derived().visit_identifier_expression(
                static_cast<const IdentifierExpression &>(expression)
            );
        case AstNodeKind::Wildcard:
            return derived().visit_wildcard_expression(
                static_cast<const WildcardExpression &>(expression)
            );
        case AstNodeKind::Literal:
            return derived().visit_literal_expression(
                static_cast<const LiteralExpression &>(expression)
            );
        case AstNodeKind::FunctionCall:
            return derived().visit_function_call_expression(
                static_cast<const FunctionCallExpression &>(expression)
            );
        case AstNodeKind::ColumnReference:
            return derived().visit_column_reference_expression(
                static_cast<const ColumnReferenceExpression &>(expression)
            );
        case AstNodeKind::Vector:
            return derived().visit_vector_expression(
                static_cast<const VectorExpression &>(expression)
            );
        case AstNodeKind::Binary:
            return derived().visit_binary_expression(
                static_cast<const BinaryExpression &>(expression)
            );
        case AstNodeKind::Unary:
            return derived().visit_unary_expression(
                static_cast<const UnaryExpression &>(expression)
            );
        case AstNodeKind::In:
            return derived().visit_in_expression(
                static_cast<const InExpression &>(expression)
            );
        case AstNodeKind::Between:
            return derived().visit_between_expression(
                static_cast<const BetweenExpression &>(expression)
            );
        case AstNodeKind::Like:
            return derived().visit_like_expression(
                static_cast<const LikeExpression &>(expression)
            );
        case AstNodeKind::Alias:
            return derived().visit_alias_expression(
                static_cast<const AliasExpression &>(expression)
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

} // namespace litedb::core::parser::ast

#pragma once

#include <utility>

#include "core/binder/bound/expression/bound_expression.hpp"
#include "core/binder/bound/expression/bound_between_expression.hpp"
#include "core/binder/bound/expression/bound_binary_expression.hpp"
#include "core/binder/bound/expression/bound_cast_expression.hpp"
#include "core/binder/bound/expression/bound_column_ref_expression.hpp"
#include "core/binder/bound/expression/bound_function_expression.hpp"
#include "core/binder/bound/expression/bound_in_expression.hpp"
#include "core/binder/bound/expression/bound_like_expression.hpp"
#include "core/binder/bound/expression/bound_literal_expression.hpp"
#include "core/binder/bound/expression/bound_null_expression.hpp"
#include "core/binder/bound/expression/bound_unary_expression.hpp"
#include "core/binder/bound/expression/bound_vector_expression.hpp"

namespace litedb::core::binder::bound
{

/**
 * @brief 绑定表达式调度器
 * @tparam Derived 派生类
 * @tparam ReturnType 返回类型，默认为 void
 */
template <typename Derived, typename ReturnType = void>
class BoundExpressionDispatcher
{
protected:
    /**
     * @brief 调度表达式
     * @param expression 表达式
     * @return 返回值
     */
    [[nodiscard]]
    ReturnType dispatch_expression(const BoundExpression & expression)
    {
        switch (expression.kind()) {
        case BoundExpressionKind::Literal:
            return derived().visit_literal_expression(
                static_cast<const BoundLiteralExpression &>(expression)
            );
        case BoundExpressionKind::Null:
            return derived().visit_null_expression(
                static_cast<const BoundNullExpression &>(expression)
            );
        case BoundExpressionKind::ColumnRef:
            return derived().visit_column_ref_expression(
                static_cast<const BoundColumnRefExpression &>(expression)
            );
        case BoundExpressionKind::Unary:
            return derived().visit_unary_expression(
                static_cast<const BoundUnaryExpression &>(expression)
            );
        case BoundExpressionKind::Binary:
            return derived().visit_binary_expression(
                static_cast<const BoundBinaryExpression &>(expression)
            );
        case BoundExpressionKind::Vector:
            return derived().visit_vector_expression(
                static_cast<const BoundVectorExpression &>(expression)
            );
        case BoundExpressionKind::Function:
            return derived().visit_function_expression(
                static_cast<const BoundFunctionExpression &>(expression)
            );
        case BoundExpressionKind::In:
            return derived().visit_in_expression(
                static_cast<const BoundInExpression &>(expression)
            );
        case BoundExpressionKind::Between:
            return derived().visit_between_expression(
                static_cast<const BoundBetweenExpression &>(expression)
            );
        case BoundExpressionKind::Like:
            return derived().visit_like_expression(
                static_cast<const BoundLikeExpression &>(expression)
            );
        case BoundExpressionKind::Cast:
            return derived().visit_cast_expression(
                static_cast<const BoundCastExpression &>(expression)
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

} // namespace litedb::core::binder::bound

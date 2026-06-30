#pragma once

#include "core/common/logical_type.hpp"
#include "core/parser/ast/ast_node.hpp"
#include "core/binder/bound/expression/bound_expression_visitor.hpp"

namespace litedb::core::binder::bound
{

/**
 * @brief 绑定表达式类型
 */
enum class BoundExpressionKind
{
    Literal,          ///< 字面量
    Null,             ///< 空值
    ColumnRef,        ///< 列引用
    Unary,            ///< 一元运算
    Binary,           ///< 二元运算
    Vector,           ///< 向量运算
    Function,         ///< 函数运算
    In,               ///< 包含运算
    Between,          ///< 范围运算
    Like,             ///< 模糊匹配运算
    Wildcard,         ///< 通配符运算
    Cast,             ///< 类型转换运算
};

/**
 * @brief 绑定表达式
 */
class BoundExpression
{
public:
    BoundExpression(const BoundExpression &) = delete;

    BoundExpression & operator=(const BoundExpression &) = delete;

    BoundExpression(BoundExpression &&) noexcept = default;

    BoundExpression & operator=(BoundExpression &&) noexcept = default;

    virtual ~BoundExpression() noexcept = default;

public:
    /**
     * @brief 获取绑定表达式类型
     * @return 绑定表达式类型
     */
    [[nodiscard]]
    BoundExpressionKind kind() const noexcept;

    /**
     * @brief 获取逻辑类型
     * @return 逻辑类型
     */
    [[nodiscard]]
    const common::LogicalType & type() const noexcept;

    /**
     * @brief 获取节点位置
     * @return 节点位置
     */
    [[nodiscard]]
    parser::ast::AstNodeLocation location() const noexcept;

    /**
     * @brief 接受访问器访问
     * @param visitor 访问器
     */
    virtual void accept(BoundExpressionVisitor & visitor) const = 0;

protected:
    BoundExpression(BoundExpressionKind kind, common::LogicalType type, parser::ast::AstNodeLocation location) noexcept;

private:
    BoundExpressionKind kind_;                  ///< 绑定表达式类型
    common::LogicalType type_;                  ///< 逻辑类型
    parser::ast::AstNodeLocation location_;     ///< 节点位置
};

} // namespace litedb::core::binder::bound

#pragma once

#include "core/common/logical_type.hpp"
#include "core/parser/ast/ast_node.hpp"

namespace litedb::core::binder::bound
{

class BoundExpressionVisitor;

enum class BoundExpressionKind
{
    Literal,
    Null,
    ColumnRef,
    Unary,
    Binary,
    Vector,
    Function,
    In,
    Between,
    Like,
    Wildcard,
    Cast,
};

class BoundExpression
{
public:
    BoundExpression(
        BoundExpressionKind kind,
        common::LogicalType type,
        parser::ast::AstNodeLocation location
    ) noexcept;

    virtual ~BoundExpression() noexcept = default;

    BoundExpression(const BoundExpression &) = delete;
    BoundExpression & operator=(const BoundExpression &) = delete;
    BoundExpression(BoundExpression &&) noexcept = default;
    BoundExpression & operator=(BoundExpression &&) noexcept = default;

    [[nodiscard]]
    BoundExpressionKind kind() const noexcept;

    [[nodiscard]]
    const common::LogicalType & type() const noexcept;

    [[nodiscard]]
    parser::ast::AstNodeLocation location() const noexcept;

    /**
     * @brief 接受访问器访问
     * @param visitor 访问器
     */
    virtual void accept(BoundExpressionVisitor & visitor) const = 0;

private:
    BoundExpressionKind kind_;
    common::LogicalType type_;
    parser::ast::AstNodeLocation location_;
};

} // namespace litedb::core::binder::bound

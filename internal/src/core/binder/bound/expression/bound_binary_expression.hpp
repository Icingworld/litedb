#pragma once

#include <memory>

#include "core/binder/bound/expression/bound_expression.hpp"
#include "core/parser/token.hpp"

namespace litedb::core::binder::bound
{

/**
 * @brief 二元表达式节点
 * @details 示例：left op right
 */
class BoundBinaryExpression final : public BoundExpression
{
public:
    BoundBinaryExpression(
        std::unique_ptr<BoundExpression> left,
        parser::TokenType op,
        std::unique_ptr<BoundExpression> right,
        common::LogicalType type,
        parser::ast::AstNodeLocation location
    );

public:
    /**
     * @brief 获取左操作数
     * @return 左操作数
     */
    [[nodiscard]]
    const BoundExpression & left() const noexcept;

    /**
     * @brief 获取操作符
     * @return 操作符
     */
    [[nodiscard]]
    parser::TokenType op() const noexcept;

    /**
     * @brief 获取右操作数
     * @return 右操作数
     */
    [[nodiscard]]
    const BoundExpression & right() const noexcept;

    /**
     * @brief 接受访问器访问
     * @param visitor 访问器
     */
    void accept(BoundExpressionVisitor & visitor) const override;

private:
    std::unique_ptr<BoundExpression> left_;     ///< 左操作数
    parser::TokenType op_;                      ///< 操作符
    std::unique_ptr<BoundExpression> right_;    ///< 右操作数
};

} // namespace litedb::core::binder::bound

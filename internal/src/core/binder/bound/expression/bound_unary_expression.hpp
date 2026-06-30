#pragma once

#include <memory>

#include "core/binder/bound/expression/bound_expression.hpp"
#include "core/parser/token.hpp"

namespace litedb::core::binder::bound
{

/**
 * @brief 一元表达式节点
 * @details 示例：op operand
 */
class BoundUnaryExpression final : public BoundExpression
{
public:
    BoundUnaryExpression(
        parser::TokenType op,
        std::unique_ptr<BoundExpression> operand,
        common::LogicalType type,
        parser::ast::AstNodeLocation location
    );

public:
    /**
     * @brief 获取操作符
     * @return 操作符
     */
    [[nodiscard]]
    parser::TokenType op() const noexcept;

    /**
     * @brief 获取操作数
     * @return 操作数
     */
    [[nodiscard]]
    const BoundExpression & operand() const noexcept;

    /**
     * @brief 接受访问器访问
     * @param visitor 访问器
     */
    void accept(BoundExpressionVisitor & visitor) const override;

private:
    parser::TokenType op_;                          ///< 操作符
    std::unique_ptr<BoundExpression> operand_;      ///< 操作数
};

} // namespace litedb::core::binder::bound

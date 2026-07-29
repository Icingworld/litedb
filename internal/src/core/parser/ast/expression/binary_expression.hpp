#pragma once

#include <memory>

#include "core/parser/ast/expression/expression_node.hpp"
#include "core/parser/token.hpp"

namespace litedb::core::parser::ast
{

/**
 * @brief 二元表达式节点
 */
class BinaryExpression final : public ExpressionNode
{
public:
    BinaryExpression(
        std::unique_ptr<ExpressionNode> left,
        TokenType op,
        std::unique_ptr<ExpressionNode> right,
        AstNodeLocation location
    ) noexcept;

public:
    /**
     * @brief 获取节点类型
     * @return 节点类型
     */
    [[nodiscard]]
    AstNodeKind kind() const noexcept override;

    /**
     * @brief 获取左操作数
     * @return 左操作数
     */
    [[nodiscard]]
    const ExpressionNode & left() const noexcept;

    /**
     * @brief 获取操作符
     * @return 操作符
     */
    [[nodiscard]]
    TokenType op() const noexcept;

    /**
     * @brief 获取右操作数
     * @return 右操作数
     */
    [[nodiscard]]
    const ExpressionNode & right() const noexcept;

private:
    std::unique_ptr<ExpressionNode> left_;      ///< 左操作数
    TokenType op_;                              ///< 操作符
    std::unique_ptr<ExpressionNode> right_;     ///< 右操作数
};

} // namespace litedb::core::parser::ast

#pragma once

#include <string>

#include "core/parser/ast/expression/expression_node.hpp"
#include "core/parser/token.hpp"

namespace litedb::core::parser::ast
{

/**
 * @brief 字面量表达式节点
 */
class LiteralExpression final : public ExpressionNode
{
public:
    LiteralExpression(
        TokenType literal_type,
        std::string value,
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
     * @brief 获取字面量类型
     * @return 字面量类型
     */
    [[nodiscard]]
    TokenType literal_type() const noexcept;

    /**
     * @brief 获取字面量值
     * @return 字面量值
     */
    [[nodiscard]]
    const std::string & value() const noexcept;

private:
    TokenType literal_type_;    // 字面量类型
    std::string value_;         // 字面量值
};

} // namespace litedb::core::parser::ast

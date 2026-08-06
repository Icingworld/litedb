#pragma once

#include <memory>
#include <vector>

#include "core/parser/ast/expression/expression_node.hpp"

namespace litedb::core::parser::ast
{

/**
 * @brief IN 表达式节点
 */
class InExpression final : public ExpressionNode
{
public:
    using ValueList = std::vector<std::unique_ptr<ExpressionNode>>;

public:
    InExpression(
        std::unique_ptr<ExpressionNode> expression,
        ValueList values,
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
     * @brief 获取表达式
     * @return 表达式
     */
    [[nodiscard]]
    const ExpressionNode & expression() const noexcept;

    /**
     * @brief 获取值列表
     * @return 值列表
     */
    [[nodiscard]]
    const ValueList & values() const noexcept;

private:
    std::unique_ptr<ExpressionNode> expression_;    ///< 表达式
    ValueList values_;                              ///< 值列表
};

} // namespace litedb::core::parser::ast

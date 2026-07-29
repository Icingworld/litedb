#pragma once

#include <memory>
#include <string>

#include "core/parser/ast/expression/expression_node.hpp"

namespace litedb::core::parser::ast
{

/**
 * @brief AS 表达式节点
 */
class AliasExpression final : public ExpressionNode
{
public:
    AliasExpression(
        std::unique_ptr<ExpressionNode> expression,
        std::string alias,
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
     * @brief 获取别名
     * @return 别名
     */
    [[nodiscard]]
    const std::string & alias() const noexcept;

private:
    std::unique_ptr<ExpressionNode> expression_;  ///< 表达式
    std::string alias_;                           ///< 别名
};

} // namespace litedb::core::parser::ast

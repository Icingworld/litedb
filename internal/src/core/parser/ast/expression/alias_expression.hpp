#pragma once

#include <memory>
#include <string>

#include "core/parser/ast/expression/expression_node.hpp"

namespace litedb::core::parser::ast
{

// AS 表达式节点
class AliasExpression final : public ExpressionNode
{
public:
    AliasExpression(
        std::unique_ptr<ExpressionNode> expression,
        std::string alias,
        AstNodeLocation location
    );

public:
    // 获取节点类型
    [[nodiscard]]
    AstNodeKind kind() const noexcept override;

    // 获取表达式
    [[nodiscard]]
    const ExpressionNode & expression() const noexcept;

    // 获取别名
    [[nodiscard]]
    const std::string & alias() const noexcept;

private:
    std::unique_ptr<ExpressionNode> expression_;
    std::string alias_;
};

} // namespace litedb::core::parser::ast

#pragma once

#include <string>

#include "core/parser/ast/expression/expression_node.hpp"

namespace litedb::core::parser::ast
{

// 标识符表达式节点
class IdentifierExpression final : public ExpressionNode
{
public:
    IdentifierExpression(std::string name, AstNodeLocation location);

public:
    // 获取节点类型
    [[nodiscard]]
    AstNodeKind kind() const noexcept override;

    // 获取标识符名称
    [[nodiscard]]
    const std::string & name() const noexcept;

private:
    std::string name_;      // 标识符名称
};

} // namespace litedb::core::parser::ast

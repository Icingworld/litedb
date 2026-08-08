#pragma once

#include <memory>
#include <string>
#include <vector>

#include "core/parser/ast/expression/expression_node.hpp"

namespace litedb::core::parser::ast
{

// 函数调用表达式节点
class FunctionCallExpression final : public ExpressionNode
{
public:
    FunctionCallExpression(
        std::string name,
        std::vector<std::unique_ptr<ExpressionNode>> arguments,
        AstNodeLocation location
    );

public:
    // 获取节点类型
    [[nodiscard]]
    AstNodeKind kind() const noexcept override;

    // 获取函数名称
    [[nodiscard]]
    const std::string & name() const noexcept;

    // 获取参数列表
    [[nodiscard]]
    const std::vector<std::unique_ptr<ExpressionNode>> & arguments() const noexcept;

private:
    std::string name_;                                          // 函数名称
    std::vector<std::unique_ptr<ExpressionNode>> arguments_;    // 参数列表
};

} // namespace litedb::core::parser::ast

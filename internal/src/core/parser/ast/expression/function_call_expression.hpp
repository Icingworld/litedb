#pragma once

#include <memory>
#include <string>
#include <vector>

#include "core/parser/ast/expression/expression_node.hpp"

namespace litedb::core::parser::ast
{

/**
 * @brief 函数调用表达式节点
 */
class FunctionCallExpression final : public ExpressionNode
{
public:
    using ArgumentList = std::vector<std::unique_ptr<ExpressionNode>>;

public:
    FunctionCallExpression(
        std::string name,
        ArgumentList arguments,
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
     * @brief 获取函数名称
     * @return 函数名称
     */
    [[nodiscard]]
    const std::string & name() const noexcept;

    /**
     * @brief 获取参数列表
     * @return 参数列表
     */
    [[nodiscard]]
    const ArgumentList & arguments() const noexcept;

private:
    std::string name_;          // 函数名称
    ArgumentList arguments_;    // 参数列表
};

} // namespace litedb::core::parser::ast

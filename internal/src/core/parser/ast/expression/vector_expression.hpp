#pragma once

#include <memory>
#include <vector>

#include "core/parser/ast/expression/expression_node.hpp"

namespace litedb::core::parser::ast
{

// 向量表达式节点
class VectorExpression final : public ExpressionNode
{
public:
    VectorExpression(
        std::vector<std::unique_ptr<ExpressionNode>> elements,
        AstNodeLocation location
    );

public:
    // 获取节点类型
    [[nodiscard]]
    AstNodeKind kind() const noexcept override;

    // 获取元素列表
    [[nodiscard]]
    const std::vector<std::unique_ptr<ExpressionNode>> & elements() const noexcept;

private:
    std::vector<std::unique_ptr<ExpressionNode>> elements_;
};

} // namespace litedb::core::parser::ast

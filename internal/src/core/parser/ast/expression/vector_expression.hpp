#pragma once

#include <memory>
#include <vector>

#include "core/parser/ast/expression/expression_node.hpp"

namespace litedb::core::parser::ast
{

/**
 * @brief 向量表达式节点
 */
class VectorExpression final : public ExpressionNode
{
public:
    /**
     * @brief 元素列表类型
     */
    using ElementList = std::vector<std::unique_ptr<ExpressionNode>>;

public:
    VectorExpression(ElementList elements, AstNodeLocation location) noexcept;

public:
    /**
     * @brief 获取节点类型
     * @return 节点类型
     */
    [[nodiscard]]
    AstNodeKind kind() const noexcept override;

    /**
     * @brief 获取元素列表
     * @return 元素列表
     */
    [[nodiscard]]
    const ElementList & elements() const noexcept;

private:
    ElementList elements_;            ///< 元素列表
};

} // namespace litedb::core::parser::ast

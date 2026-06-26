#pragma once

#include <optional>
#include <string>

#include "core/parser/ast/expression/expression_node.hpp"

namespace litedb::core::parser::ast
{

/**
 * @brief 通配符表达式节点
 * @details 示例：*
 */
class WildcardExpression final : public ExpressionNode
{
public:
    explicit WildcardExpression(AstNodeLocation location) noexcept;

    WildcardExpression(std::optional<std::string> qualifier, AstNodeLocation location) noexcept;

public:
    /**
     * @brief 获取节点类型
     * @return 节点类型
     */
    [[nodiscard]]
    AstNodeKind kind() const noexcept override;

    /**
     * @brief 接受访问器访问
     * @param visitor 访问器
     */
    void accept(AstNodeVisitor & visitor) const override;

    /**
     * @brief 获取限定符
     * @return 限定符
     */
    [[nodiscard]]
    const std::optional<std::string> & qualifier() const noexcept;

private:
    std::optional<std::string> qualifier_;    ///< 限定符
};

} // namespace litedb::core::parser::ast

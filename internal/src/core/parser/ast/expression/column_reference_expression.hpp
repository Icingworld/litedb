#pragma once

#include <optional>
#include <string>

#include "core/parser/ast/expression/expression_node.hpp"

namespace litedb::core::parser::ast
{

/**
 * @brief 列引用表达式节点
 * @details 示例：qualifier.column
 */
class ColumnReferenceExpression final : public ExpressionNode
{
public:
    ColumnReferenceExpression(
        std::optional<std::string> qualifier,
        std::string column_name,
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
     * @brief 获取限定符
     * @return 限定符
     */
    [[nodiscard]]
    const std::optional<std::string> & qualifier() const noexcept;

    /**
     * @brief 获取列名
     * @return 列名
     */
    [[nodiscard]]
    const std::string & column_name() const noexcept;

private:
    std::optional<std::string> qualifier_;    // 限定符
    std::string column_name_;                 // 列名
};

} // namespace litedb::core::parser::ast

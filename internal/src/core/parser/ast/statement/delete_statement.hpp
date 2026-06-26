#pragma once

#include <memory>
#include <string>

#include "core/parser/ast/expression/expression_node.hpp"
#include "core/parser/ast/statement/statement_node.hpp"

namespace litedb::core::parser::ast
{

/**
 * @brief DELETE 语句节点
 * @details 示例：DELETE FROM <collection> [WHERE <condition>]
 */
class DeleteStatement final : public StatementNode
{
public:
    DeleteStatement(std::string collection, std::unique_ptr<ExpressionNode> where, AstNodeLocation location) noexcept;

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
     * @brief 获取集合名称
     * @return 集合名称
     */
    [[nodiscard]]
    const std::string & collection() const noexcept;

    /**
     * @brief 获取条件表达式
     * @return 条件表达式
     */
    [[nodiscard]]
    const ExpressionNode * where() const noexcept;

private:
    std::string collection_;                        ///< 集合名称
    std::unique_ptr<ExpressionNode> where_;         ///< 条件表达式
};

} // namespace litedb::core::parser::ast

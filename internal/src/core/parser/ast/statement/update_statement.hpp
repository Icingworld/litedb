#pragma once

#include <memory>
#include <string>
#include <vector>

#include "core/parser/ast/expression/expression_node.hpp"
#include "core/parser/ast/statement/statement_node.hpp"

namespace litedb::core::parser::ast
{

/**
 * @brief 赋值
 * @note 示例：column = value
 */
struct Assignment
{
    std::string column;
    std::unique_ptr<ExpressionNode> value;
};

/**
 * @brief UPDATE 语句节点
 * @details 示例：UPDATE <collection_name> SET <assignment> [WHERE <expression>]
 */
class UpdateStatement final : public StatementNode
{
public:
    using AssignmentList = std::vector<Assignment>;

public:
    UpdateStatement(
        std::string collection,
        AssignmentList assignments,
        std::unique_ptr<ExpressionNode> where,
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
     * @brief 获取赋值列表
     * @return 赋值列表
     */
    [[nodiscard]]
    const AssignmentList & assignments() const noexcept;

    /**
     * @brief 获取条件表达式
     * @return 条件表达式
     */
    [[nodiscard]]
    const ExpressionNode * where() const noexcept;

private:
    std::string collection_;                ///< 集合名称
    AssignmentList assignments_;            ///< 赋值列表
    std::unique_ptr<ExpressionNode> where_; ///< 条件表达式
};

} // namespace litedb::core::parser::ast

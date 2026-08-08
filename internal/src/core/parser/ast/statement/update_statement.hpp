#pragma once

#include <memory>
#include <string>
#include <vector>
#include <optional>

#include "core/parser/ast/statement/statement_node.hpp"
#include "core/parser/ast/expression/expression_node.hpp"

namespace litedb::core::parser::ast
{

// 赋值项
struct Assignment
{
    std::string column_name;                        // 列名
    std::unique_ptr<ExpressionNode> value;          // 值
};

// UPDATE 语句节点
class UpdateStatement final : public StatementNode
{
public:
    UpdateStatement(
        std::string collection_name,
        std::vector<Assignment> assignments,
        std::unique_ptr<ExpressionNode> where,
        AstNodeLocation location
    );

public:
    // 获取节点类型
    [[nodiscard]]
    AstNodeKind kind() const noexcept override;

    // 获取集合名称
    [[nodiscard]]
    const std::string & collection_name() const noexcept;

    // 获取赋值列表
    [[nodiscard]]
    const std::vector<Assignment> & assignments() const noexcept;

    // 获取条件表达式
    [[nodiscard]]
    std::optional<const ExpressionNode &> where() const noexcept;

private:
    std::string collection_name_;           // 集合名称
    std::vector<Assignment> assignments_;   // 赋值列表
    std::unique_ptr<ExpressionNode> where_; // 条件表达式
};

} // namespace litedb::core::parser::ast

#pragma once

#include <memory>
#include <optional>
#include <string>

#include "core/parser/ast/expression/expression_node.hpp"
#include "core/parser/ast/statement/statement_node.hpp"

namespace litedb::core::parser::ast
{

// DELETE 语句节点
class DeleteStatement final : public StatementNode
{
public:
    DeleteStatement(
        std::string collection_name,
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

    // 获取条件表达式
    [[nodiscard]]
    std::optional<const ExpressionNode &> where() const noexcept;

private:
    std::string collection_name_;
    std::unique_ptr<ExpressionNode> where_;
};

} // namespace litedb::core::parser::ast

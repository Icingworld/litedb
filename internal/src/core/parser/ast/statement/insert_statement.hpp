#pragma once

#include <memory>
#include <string>
#include <vector>

#include "core/parser/ast/expression/expression_node.hpp"
#include "core/parser/ast/statement/statement_node.hpp"

namespace litedb::core::parser::ast
{

// INSERT 语句节点
class InsertStatement final : public StatementNode
{
public:
    InsertStatement(
        std::string collection_name,
        std::vector<std::string> columns,
        std::vector<std::unique_ptr<ExpressionNode>> values,
        AstNodeLocation location
    );

public:
    // 获取节点类型
    [[nodiscard]]
    AstNodeKind kind() const noexcept override;

    // 获取集合名称
    [[nodiscard]]
    const std::string & collection_name() const noexcept;

    // 获取列列表
    [[nodiscard]]
    const std::vector<std::string> & columns() const noexcept;

    // 获取值列表
    [[nodiscard]]
    const std::vector<std::unique_ptr<ExpressionNode>> & values() const noexcept;

private:
    std::string collection_name_;                               // 集合名称
    std::vector<std::string> columns_;                          // 列列表
    std::vector<std::unique_ptr<ExpressionNode>> values_;       // 值列表
};

} // namespace litedb::core::parser::ast

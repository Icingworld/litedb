#pragma once

#include <memory>
#include <string>
#include <vector>

#include "core/parser/ast/expression/expression_node.hpp"
#include "core/parser/ast/statement/statement_node.hpp"

namespace litedb::core::parser::ast
{

class InsertStatement final : public StatementNode
{
public:
    using ColumnList = std::vector<std::string>;
    using ValueList = std::vector<std::unique_ptr<ExpressionNode>>;

    InsertStatement(std::string collection, ColumnList columns, ValueList values, AstNodeLocation location) noexcept;

    [[nodiscard]]
    AstNodeKind kind() const noexcept override;

    [[nodiscard]]
    const std::string & collection() const noexcept;

    [[nodiscard]]
    const ColumnList & columns() const noexcept;

    [[nodiscard]]
    const ValueList & values() const noexcept;

private:
    std::string collection_;
    ColumnList columns_;
    ValueList values_;
};

} // namespace litedb::core::parser::ast

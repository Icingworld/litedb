#pragma once

#include <memory>
#include <string>
#include <vector>

#include "core/parser/ast/expression/expression_node.hpp"
#include "core/parser/ast/statement/statement_node.hpp"

namespace litedb::core::parser::ast
{

struct Assignment
{
    std::string column;
    std::unique_ptr<ExpressionNode> value;
};

class UpdateStatement final : public StatementNode
{
public:
    using AssignmentList = std::vector<Assignment>;

    UpdateStatement(
        std::string collection,
        AssignmentList assignments,
        std::unique_ptr<ExpressionNode> where,
        AstNodeLocation location
    ) noexcept;

    [[nodiscard]]
    AstNodeKind kind() const noexcept override;

    [[nodiscard]]
    const std::string & collection() const noexcept;

    [[nodiscard]]
    const AssignmentList & assignments() const noexcept;

    [[nodiscard]]
    const ExpressionNode * where() const noexcept;

private:
    std::string collection_;
    AssignmentList assignments_;
    std::unique_ptr<ExpressionNode> where_;
};

} // namespace litedb::core::parser::ast

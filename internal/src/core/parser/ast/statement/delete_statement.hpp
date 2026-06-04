#pragma once

#include <memory>
#include <string>

#include "core/parser/ast/expression/expression_node.hpp"
#include "core/parser/ast/statement/statement_node.hpp"

namespace litedb::core::parser::ast
{

class DeleteStatement final : public StatementNode
{
public:
    DeleteStatement(std::string collection, std::unique_ptr<ExpressionNode> where, AstNodeLocation location) noexcept;

    [[nodiscard]]
    AstNodeKind kind() const noexcept override;

    [[nodiscard]]
    const std::string & collection() const noexcept;

    [[nodiscard]]
    const ExpressionNode * where() const noexcept;

private:
    std::string collection_;
    std::unique_ptr<ExpressionNode> where_;
};

} // namespace litedb::core::parser::ast

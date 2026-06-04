#pragma once

#include "core/parser/ast/statement/statement_node.hpp"
#include "core/parser/token.hpp"

namespace litedb::core::parser::ast
{

class ShowStatement final : public StatementNode
{
public:
    ShowStatement(TokenType object_type, AstNodeLocation location) noexcept;

    [[nodiscard]]
    AstNodeKind kind() const noexcept override;

    [[nodiscard]]
    TokenType object_type() const noexcept;

private:
    TokenType object_type_;
};

} // namespace litedb::core::parser::ast

#pragma once

#include <string>

#include "core/parser/ast/statement/statement_node.hpp"
#include "core/parser/token.hpp"

namespace litedb::core::parser::ast
{

class AlterStatement final : public StatementNode
{
public:
    AlterStatement(TokenType object_type, std::string name, AstNodeLocation location) noexcept;

    [[nodiscard]]
    AstNodeKind kind() const noexcept override;

    [[nodiscard]]
    TokenType object_type() const noexcept;

    [[nodiscard]]
    const std::string & name() const noexcept;

private:
    TokenType object_type_;
    std::string name_;
};

} // namespace litedb::core::parser::ast

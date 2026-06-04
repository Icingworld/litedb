#pragma once

#include <string>

#include "core/parser/ast/statement/statement_node.hpp"
#include "core/parser/token.hpp"

namespace litedb::core::parser::ast
{

class DropStatement final : public StatementNode
{
public:
    DropStatement(TokenType object_type, std::string name, bool if_exists, AstNodeLocation location) noexcept;

    [[nodiscard]]
    AstNodeKind kind() const noexcept override;

    [[nodiscard]]
    TokenType object_type() const noexcept;

    [[nodiscard]]
    const std::string & name() const noexcept;

    [[nodiscard]]
    bool if_exists() const noexcept;

private:
    TokenType object_type_;
    std::string name_;
    bool if_exists_;
};

} // namespace litedb::core::parser::ast

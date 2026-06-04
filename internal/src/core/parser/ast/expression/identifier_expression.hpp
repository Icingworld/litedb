#pragma once

#include <string>

#include "core/parser/ast/expression/expression_node.hpp"

namespace litedb::core::parser::ast
{

class IdentifierExpression final : public ExpressionNode
{
public:
    IdentifierExpression(std::string name, AstNodeLocation location) noexcept;

    [[nodiscard]]
    AstNodeKind kind() const noexcept override;

    [[nodiscard]]
    const std::string & name() const noexcept;

private:
    std::string name_;
};

} // namespace litedb::core::parser::ast

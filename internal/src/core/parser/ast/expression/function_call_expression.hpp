#pragma once

#include <memory>
#include <string>
#include <vector>

#include "core/parser/ast/expression/expression_node.hpp"

namespace litedb::core::parser::ast
{

class FunctionCallExpression final : public ExpressionNode
{
public:
    using ArgumentList = std::vector<std::unique_ptr<ExpressionNode>>;

    FunctionCallExpression(std::string name, ArgumentList arguments, AstNodeLocation location) noexcept;

    [[nodiscard]]
    AstNodeKind kind() const noexcept override;

    [[nodiscard]]
    const std::string & name() const noexcept;

    [[nodiscard]]
    const ArgumentList & arguments() const noexcept;

private:
    std::string name_;
    ArgumentList arguments_;
};

} // namespace litedb::core::parser::ast

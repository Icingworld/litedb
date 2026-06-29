#include "core/parser/ast/expression/function_call_expression.hpp"

#include <utility>

namespace litedb::core::parser::ast
{

FunctionCallExpression::FunctionCallExpression(std::string name, ArgumentList arguments, AstNodeLocation location) noexcept
    : ExpressionNode(location)
    , name_(std::move(name))
    , arguments_(std::move(arguments))
{
}

AstNodeKind FunctionCallExpression::kind() const noexcept
{
    return AstNodeKind::FunctionCall;
}

void FunctionCallExpression::accept(AstNodeVisitor & visitor) const
{
    visitor.visit(*this);
}

const std::string & FunctionCallExpression::name() const noexcept
{
    return name_;
}

const FunctionCallExpression::ArgumentList & FunctionCallExpression::arguments() const noexcept
{
    return arguments_;
}

} // namespace litedb::core::parser::ast

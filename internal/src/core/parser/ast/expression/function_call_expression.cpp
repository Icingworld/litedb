#include "core/parser/ast/expression/function_call_expression.hpp"

#include <utility>
#include <cassert>

namespace litedb::core::parser::ast
{

FunctionCallExpression::FunctionCallExpression(
    std::string name,
    std::vector<std::unique_ptr<ExpressionNode>> arguments,
    AstNodeLocation location
)
    : ExpressionNode(location)
    , name_(std::move(name))
    , arguments_(std::move(arguments))
{
    assert(!name_.empty());
    for (const auto & argument : arguments_) {
        assert(argument != nullptr);
    }
}

AstNodeKind FunctionCallExpression::kind() const noexcept
{
    return AstNodeKind::FunctionCall;
}

const std::string & FunctionCallExpression::name() const noexcept
{
    return name_;
}

const std::vector<std::unique_ptr<ExpressionNode>> &
FunctionCallExpression::arguments() const noexcept
{
    return arguments_;
}

} // namespace litedb::core::parser::ast

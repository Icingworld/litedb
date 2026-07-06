#include "core/binder/bound/expression/bound_function_expression.hpp"

#include <memory>
#include <utility>

namespace litedb::core::binder::bound
{

BoundFunctionExpression::BoundFunctionExpression(
    std::string name,
    std::shared_ptr<const function::ScalarFunction> function,
    function::FunctionSignature signature,
    std::vector<std::unique_ptr<BoundExpression>> arguments,
    common::LogicalType type,
    parser::ast::AstNodeLocation location
)
    : BoundExpression(BoundExpressionKind::Function, type, location)
    , name_(std::move(name))
    , function_(std::move(function))
    , signature_(std::move(signature))
    , arguments_(std::move(arguments))
{
}

const std::string & BoundFunctionExpression::name() const noexcept
{
    return name_;
}

const function::ScalarFunction & BoundFunctionExpression::function() const noexcept
{
    return *function_;
}

const function::FunctionSignature & BoundFunctionExpression::signature() const noexcept
{
    return signature_;
}

const std::vector<std::unique_ptr<BoundExpression>> & BoundFunctionExpression::arguments() const noexcept
{
    return arguments_;
}

void BoundFunctionExpression::accept(BoundExpressionVisitor & visitor) const
{
    visitor.visit(*this);
}

std::unique_ptr<BoundExpression> BoundFunctionExpression::clone() const
{
    std::vector<std::unique_ptr<BoundExpression>> arguments;
    arguments.reserve(arguments_.size());
    for (const auto & argument : arguments_) {
        arguments.push_back(argument->clone());
    }
    return std::make_unique<BoundFunctionExpression>(
        name_,
        function_,
        signature_,
        std::move(arguments),
        type(),
        location()
    );
}

} // namespace litedb::core::binder::bound

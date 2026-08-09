#include "core/binder/bound/expression/bound_function_expression.hpp"

#include <utility>

namespace litedb::core::binder::bound
{

BoundFunctionExpression::BoundFunctionExpression(
    function::BoundScalarFunction function,
    std::vector<std::unique_ptr<BoundExpression>> arguments
)
    : BoundExpression(BoundExpressionKind::Function, function.return_type())
    , function_(std::move(function))
    , arguments_(std::move(arguments))
{}

const function::BoundScalarFunction & BoundFunctionExpression::function() const noexcept
{
    return function_;
}

const std::vector<std::unique_ptr<BoundExpression>> &
BoundFunctionExpression::arguments() const noexcept
{
    return arguments_;
}

std::vector<std::unique_ptr<BoundExpression>> BoundFunctionExpression::take_arguments() noexcept
{
    return std::move(arguments_);
}

} // namespace litedb::core::binder::bound

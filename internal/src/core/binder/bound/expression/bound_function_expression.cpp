#include "core/binder/bound/expression/bound_function_expression.hpp"

#include <utility>

namespace litedb::core::binder::bound
{

BoundFunctionExpression::BoundFunctionExpression(
    std::shared_ptr<const function::ScalarFunction> function,
    std::vector<std::unique_ptr<BoundExpression>> arguments,
    common::LogicalType return_type
)
    : BoundExpression(BoundExpressionKind::Function, return_type)
    , function_(std::move(function))
    , arguments_(std::move(arguments))
{
}

const function::ScalarFunction &
BoundFunctionExpression::function() const noexcept
{
    return *function_;
}

const std::vector<std::unique_ptr<BoundExpression>> &
BoundFunctionExpression::arguments() const noexcept
{
    return arguments_;
}

} // namespace litedb::core::binder::bound

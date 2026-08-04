#include "core/function/scalar_function.hpp"

#include <utility>

namespace litedb::core::function
{

ScalarFunction::ScalarFunction(
    std::string name,
    std::vector<FunctionSignature> signatures,
    EvalFn eval
)
    : Function(std::move(name), FunctionKind::Scalar)
    , signatures_(std::move(signatures))
    , eval_(eval)
{
}

const std::vector<FunctionSignature> & ScalarFunction::signatures() const noexcept
{
    return signatures_;
}

std::expected<common::Value, FunctionError> ScalarFunction::evaluate(
    const std::vector<common::Value> & arguments,
    const ScalarFunctionContext & context
) const
{
    return eval_(arguments, context);
}

} // namespace litedb::core::function

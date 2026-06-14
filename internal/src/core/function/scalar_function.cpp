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

std::expected<schema::Value, FunctionError> ScalarFunction::evaluate(
    const std::vector<schema::Value> & arguments,
    const ScalarFunctionContext & context,
    parser::ast::AstNodeLocation location
) const
{
    return eval_(arguments, context, location);
}

} // namespace litedb::core::function

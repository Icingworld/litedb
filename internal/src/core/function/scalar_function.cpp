#include "core/function/scalar_function.hpp"

#include <utility>

namespace litedb::core::function
{

BoundScalarFunction::BoundScalarFunction(
    std::string name,
    std::shared_ptr<const ScalarFunctionOverload> overload,
    std::vector<common::LogicalType> argument_types,
    common::LogicalType return_type,
    std::shared_ptr<const FunctionBindData> bind_data,
    std::size_t match_cost
)
    : name_(std::move(name))
    , overload_(std::move(overload))
    , argument_types_(std::move(argument_types))
    , return_type_(return_type)
    , bind_data_(std::move(bind_data))
    , match_cost_(match_cost)
{
}

const ScalarFunctionOverload & BoundScalarFunction::overload() const noexcept
{
    return *overload_;
}

const std::string & BoundScalarFunction::name() const noexcept
{
    return name_;
}

const std::vector<common::LogicalType> &
BoundScalarFunction::argument_types() const noexcept
{
    return argument_types_;
}

const common::LogicalType & BoundScalarFunction::return_type() const noexcept
{
    return return_type_;
}

const FunctionProperties & BoundScalarFunction::properties() const noexcept
{
    return overload_->properties;
}

std::size_t BoundScalarFunction::match_cost() const noexcept
{
    return match_cost_;
}

std::expected<common::Value, FunctionError> BoundScalarFunction::evaluate(
    std::span<const common::Value> arguments,
    const ScalarFunctionContext & context
) const
{
    if (properties().null_handling == FunctionNullHandling::PropagateNull) {
        for (const auto & argument : arguments) {
            if (argument.is_null()) {
                return common::Value::null();
            }
        }
    }
    return overload_->evaluate(arguments, context, bind_data_.get());
}

} // namespace litedb::core::function

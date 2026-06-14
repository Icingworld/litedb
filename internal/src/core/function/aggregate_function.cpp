#include "core/function/aggregate_function.hpp"

#include <utility>

namespace litedb::core::function
{

AggregateFunction::AggregateFunction(std::string name)
    : Function(std::move(name), FunctionKind::Aggregate)
{
}

} // namespace litedb::core::function

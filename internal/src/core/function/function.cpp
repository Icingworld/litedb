#include "core/function/function.hpp"

#include <utility>

namespace litedb::core::function
{

Function::Function(std::string name, FunctionKind kind)
    : name_(std::move(name))
    , kind_(kind)
{
}

const std::string & Function::name() const noexcept
{
    return name_;
}

FunctionKind Function::kind() const noexcept
{
    return kind_;
}

} // namespace litedb::core::function

#include "core/function/builtin/builtin_functions.hpp"

#include "core/function/builtin/vector_functions.hpp"

namespace litedb::core::function::builtin
{

void register_builtin_functions(FunctionRegistry & registry)
{
    register_vector_functions(registry);
}

FunctionRegistry make_builtin_function_registry()
{
    FunctionRegistry registry;
    register_builtin_functions(registry);
    return registry;
}

} // namespace litedb::core::function::builtin

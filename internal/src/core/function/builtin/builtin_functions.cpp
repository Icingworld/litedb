#include "core/function/builtin/builtin_functions.hpp"

#include <exception>
#include <utility>

#include "core/function/builtin/vector_functions.hpp"

namespace litedb::core::function::builtin
{

std::expected<void, FunctionError> register_builtin_functions(FunctionCatalogBuilder & builder)
{
    return register_vector_functions(builder);
}

std::expected<FunctionCatalog, FunctionError> make_builtin_function_catalog()
{
    FunctionCatalogBuilder builder;
    if (auto result = register_builtin_functions(builder); !result.has_value()) {
        return std::unexpected(std::move(result.error()));
    }
    return std::move(builder).build();
}

const FunctionCatalog & builtin_function_catalog()
{
    static const FunctionCatalog catalog = [] {
        auto result = make_builtin_function_catalog();
        if (!result.has_value()) {
            std::terminate();
        }
        return std::move(*result);
    }();
    return catalog;
}

} // namespace litedb::core::function::builtin

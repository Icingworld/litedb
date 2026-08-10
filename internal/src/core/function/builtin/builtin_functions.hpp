#pragma once

#include <expected>

#include "core/function/function_catalog.hpp"

namespace litedb::core::function::builtin
{

[[nodiscard]]
std::expected<void, FunctionError> register_builtin_functions(FunctionCatalogBuilder & builder);

[[nodiscard]]
std::expected<FunctionCatalog, FunctionError> make_builtin_function_catalog();

[[nodiscard]]
const FunctionCatalog & builtin_function_catalog();

} // namespace litedb::core::function::builtin

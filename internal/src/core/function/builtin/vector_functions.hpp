#pragma once

#include <expected>

#include "core/function/function_catalog.hpp"

namespace litedb::core::function::builtin
{

[[nodiscard]]
std::expected<void, FunctionError> register_vector_functions(FunctionCatalogBuilder & builder);

} // namespace litedb::core::function::builtin

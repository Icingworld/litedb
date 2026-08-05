#include "core/function/function_helper.hpp"

namespace litedb::core::function
{

FunctionError make_error(FunctionErrorCode code, std::string_view message)
{
    return FunctionError {code, std::string(message)};
}

} // namespace litedb::core::function
#include "core/meta/meta_error.hpp"

#include <utility>

namespace litedb::core::meta
{

MetaError make_error(MetaErrorCode code, std::string message, MetaErrorContext context)
{
    return MetaError {code, message, std::move(context)};
}

} // namespace litedb::core::meta

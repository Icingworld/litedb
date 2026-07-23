#include "core/meta/meta_helper.hpp"

namespace litedb::core::meta
{

MetaStoreError make_error(MetaStoreErrorCode code, std::string message)
{
    return MetaStoreError {code, std::move(message)};
}

} // namespace litedb::core::meta

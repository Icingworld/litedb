#include "core/meta/meta_helper.hpp"

#include <cctype>

namespace litedb::core::meta
{

std::string normalize_identifier(std::string_view name)
{
    std::string key;
    key.reserve(name.size());
    for (const unsigned char ch : name) {
        key.push_back(static_cast<char>(std::tolower(ch)));
    }
    return key;
}

MetaStoreError make_error(MetaStoreErrorCode code, std::string message)
{
    return MetaStoreError {code, std::move(message)};
}

} // namespace litedb::core::meta

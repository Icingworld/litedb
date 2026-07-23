#include "core/common/identifier.hpp"

#include <cctype>

namespace litedb::core::common
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

} // namespace litedb::core::common

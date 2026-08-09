#pragma once

#include <string>
#include <string_view>

namespace litedb::core::common
{

// 规范化标识符
[[nodiscard]]
std::string normalize_identifier(std::string_view name);

} // namespace litedb::core::common

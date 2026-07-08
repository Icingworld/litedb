#pragma once

#include <string>
#include <string_view>

namespace litedb::core::meta
{

/**
 * @brief 规范化标识符
 * @param name 标识符
 * @return 规范化后的标识符
 */
[[nodiscard]]
std::string normalize_identifier(std::string_view name);

} // namespace litedb::core::meta

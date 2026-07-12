#pragma once

#include <string>
#include <string_view>

#include "core/meta/meta_store_error.hpp"

namespace litedb::core::meta
{

/**
 * @brief 规范化标识符
 * @param name 标识符
 * @return 规范化后的标识符
 */
[[nodiscard]]
std::string normalize_identifier(std::string_view name);

/**
 * @brief 创建元数据存储错误
 * @param code 错误码
 * @param message 错误消息
 * @return 元数据存储错误
 */
[[nodiscard]]
MetaStoreError make_error(MetaStoreErrorCode code, std::string message);

} // namespace litedb::core::meta

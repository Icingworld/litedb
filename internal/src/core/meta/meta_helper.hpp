#pragma once

#include <string>
#include "core/common/identifier.hpp"
#include "core/meta/meta_store_error.hpp"

namespace litedb::core::meta
{

/**
 * @brief 创建元数据存储错误
 * @param code 错误码
 * @param message 错误消息
 * @return 元数据存储错误
 */
[[nodiscard]]
MetaStoreError make_error(MetaStoreErrorCode code, std::string message);

} // namespace litedb::core::meta

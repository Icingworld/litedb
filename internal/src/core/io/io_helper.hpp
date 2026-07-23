#pragma once

#include <string_view>

#include "core/io/io_error.hpp"

namespace litedb::core::io
{

/**
 * @brief 创建 IO 错误
 * @param code 错误码
 * @param message 错误消息
 * @return IO 错误
 */
IoError make_io_error(IoErrorCode code, std::string_view message);

} // namespace litedb::core::io

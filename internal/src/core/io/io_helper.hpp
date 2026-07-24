#pragma once

#include "core/io/io_error.hpp"

namespace litedb::core::io
{

/**
 * @brief 创建 IO 错误
 * @param code 错误码
 * @param message 错误消息
 * @return IO 错误
 */
IOError make_io_error(IOErrorCode code, const std::string & message);

} // namespace litedb::core::io

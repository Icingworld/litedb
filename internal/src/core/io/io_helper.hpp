#pragma once

#include <string_view>

#include "core/io/io_error.hpp"

namespace litedb::core::io
{

// 创建 IO 错误
[[nodiscard]]
IoError make_io_error(IoErrorCode code, std::string_view message);

} // namespace litedb::core::io

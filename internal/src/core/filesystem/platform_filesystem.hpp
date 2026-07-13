#pragma once

#include "core/filesystem/filesystem.hpp"

namespace litedb::core::filesystem
{

/**
 * @brief 创建当前平台的文件系统
 */
[[nodiscard]]
FileSystem create_platform_filesystem();

} // namespace litedb::core::filesystem

#pragma once

#include <memory>

#include "core/filesystem/backend/filesystem_backend.hpp"

namespace litedb::core::filesystem::backend
{

// 创建当前平台的文件系统后端
[[nodiscard]]
std::unique_ptr<FileSystemBackend> create_platform_filesystem_backend();

} // namespace litedb::core::filesystem::backend

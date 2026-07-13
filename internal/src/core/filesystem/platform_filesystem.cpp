#include "core/filesystem/platform_filesystem.hpp"

#include "core/filesystem/backend/platform_filesystem_backend.hpp"

namespace litedb::core::filesystem
{

FileSystem create_platform_filesystem()
{
    return FileSystem {backend::create_platform_filesystem_backend()};
}

} // namespace litedb::core::filesystem

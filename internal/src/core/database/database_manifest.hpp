#pragma once

#include <expected>
#include <filesystem>
#include <string>

#include "core/error/error.hpp"
#include "core/filesystem/filesystem.hpp"

namespace litedb::core::database
{

/**
 * @brief 数据库 manifest 错误码
 */
enum class ManifestErrorCode : std::uint8_t
{
    FileSystemError,            // 文件系统错误
    InvalidFormat,              // 格式无效
};

/**
 * @brief 数据库 manifest 错误
 */
using ManifestError = error::Error;

/**
 * @brief 数据库 manifest
 */
class DatabaseManifest
{
public:
    DatabaseManifest(std::filesystem::path data_dir, filesystem::FileSystem & filesystem);

public:
    /**
     * @brief 确保 manifest 初始化
     * @return 结果
     */
    [[nodiscard]]
    std::expected<void, ManifestError> ensure_initialized() const;

    /**
     * @brief 获取数据目录
     * @return 数据目录
     */
    [[nodiscard]]
    const std::filesystem::path & data_dir() const noexcept;

    /**
     * @brief 获取 meta 文件路径
     * @return meta 文件路径
     */
    [[nodiscard]]
    std::filesystem::path meta_path() const;

    /**
     * @brief 获取 collections 目录路径
     * @return collections 目录路径
     */
    [[nodiscard]]
    std::filesystem::path collections_dir() const;

private:
    std::filesystem::path data_dir_;            // 数据目录
    filesystem::FileSystem * filesystem_;       // 文件系统
};

} // namespace litedb::core::database

namespace litedb::core::error
{
template <>
struct ErrorTraits<database::ManifestErrorCode>
{
    static constexpr ErrorCategory category = ErrorCategory::Database;
};
} // namespace litedb::core::error

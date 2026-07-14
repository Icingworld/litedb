#include "core/database/database_manifest.hpp"

#include <utility>

#include "core/database/database_format.hpp"
#include "core/io/binary_io.hpp"
#include "core/io/file_byte_reader.hpp"
#include "core/io/file_byte_writer.hpp"

namespace litedb::core::database
{

namespace
{

/**
 * @brief 创建 manifest 错误
 * @param code 错误码
 * @param message 错误消息
 * @return 错误
 */
ManifestError make_error(ManifestErrorCode code, std::string message)
{
    return ManifestError {code, std::move(message)};
}

/**
 * @brief 从文件系统错误创建 manifest 错误
 * @param error 文件系统错误
 * @return 错误
 */
ManifestError from_filesystem_error(filesystem::FileSystemError error)
{
    return ManifestError {
        .code = ManifestErrorCode::FileSystemError,
        .message = std::move(error.message),
    };
}

/**
 * @brief 从 IO 错误创建 manifest 错误
 * @param error  IO 错误
 * @return 错误
 */
ManifestError from_io_error(io::IoError error)
{
    return ManifestError {
        .code = error.code == io::IoErrorCode::FileSystemError
            ? ManifestErrorCode::FileSystemError : ManifestErrorCode::InvalidFormat,
        .message = std::move(error.message),
    };
}

/**
 * @brief 写入文件头
 * @param writer 写入器
 * @param magic 魔数
 * @return 结果
 */
std::expected<void, ManifestError> write_file_header(io::BinaryWriter & writer, std::uint32_t magic)
{
    auto magic_written = writer.write_u32(magic);
    if (!magic_written.has_value()) {
        return std::unexpected(from_io_error(std::move(magic_written.error())));
    }

    auto version_written = writer.write_u16(DatabaseFormatVersion);
    if (!version_written.has_value()) {
        return std::unexpected(from_io_error(std::move(version_written.error())));
    }

    auto header_size_written = writer.write_u16(FileHeaderSize);
    if (!header_size_written.has_value()) {
        return std::unexpected(from_io_error(std::move(header_size_written.error())));
    }

    return {};
}

/**
 * @brief 读取文件头
 * @param reader 读取器
 * @param expected_magic 期望的魔数
 * @return 结果
 */
std::expected<void, ManifestError> read_file_header(io::BinaryReader & reader, std::uint32_t expected_magic)
{
    auto magic = reader.read_u32();
    if (!magic.has_value()) {
        return std::unexpected(from_io_error(std::move(magic.error())));
    }
    if (magic.value() != expected_magic) {
        return std::unexpected(make_error(ManifestErrorCode::InvalidFormat, "Invalid file magic"));
    }

    auto version = reader.read_u16();
    if (!version.has_value()) {
        return std::unexpected(from_io_error(std::move(version.error())));
    }
    if (version.value() != DatabaseFormatVersion) {
        return std::unexpected(make_error(ManifestErrorCode::InvalidFormat, "Unsupported storage format version"));
    }

    auto header_size = reader.read_u16();
    if (!header_size.has_value()) {
        return std::unexpected(from_io_error(std::move(header_size.error())));
    }
    if (header_size.value() < FileHeaderSize) {
        return std::unexpected(make_error(ManifestErrorCode::InvalidFormat, "Invalid file header size"));
    }

    return {};
}

} // namespace

DatabaseManifest::DatabaseManifest(std::filesystem::path data_dir, filesystem::FileSystem & filesystem)
    : data_dir_(std::move(data_dir))
    , filesystem_(&filesystem)
{
}

std::expected<void, ManifestError> DatabaseManifest::ensure_initialized() const
{
    auto created = filesystem_->create_dir_all(collections_dir());
    if (!created.has_value()) {
        return std::unexpected(from_filesystem_error(std::move(created.error())));
    }

    const auto path = data_dir_ / ManifestFileName;
    auto exists = filesystem_->exists(path);
    if (!exists.has_value()) {
        return std::unexpected(from_filesystem_error(std::move(exists.error())));
    }
    if (!exists.value()) {
        auto file = filesystem_->open(
            path,
            filesystem::backend::FileOpenOptions {
                .access = filesystem::backend::FileAccess::ReadWrite,
                .create_mode = filesystem::backend::FileCreateMode::CreateOrTruncate,
            }
        );
        if (!file.has_value()) {
            return std::unexpected(from_filesystem_error(std::move(file.error())));
        }

        io::FileByteWriter byte_writer {file.value()};
        io::BinaryWriter writer {byte_writer};
        auto header_written = write_file_header(writer, ManifestMagic);
        if (!header_written.has_value()) {
            return std::unexpected(std::move(header_written.error()));
        }

        auto version_written = writer.write_u32(DatabaseFormatVersion);
        if (!version_written.has_value()) {
            return std::unexpected(from_io_error(std::move(version_written.error())));
        }
        auto meta_path_written = writer.write_string(MetaFileName);
        if (!meta_path_written.has_value()) {
            return std::unexpected(from_io_error(std::move(meta_path_written.error())));
        }
        auto collections_path_written = writer.write_string(CollectionsDirName);
        if (!collections_path_written.has_value()) {
            return std::unexpected(from_io_error(std::move(collections_path_written.error())));
        }

        auto synced = file->sync_all();
        if (!synced.has_value()) {
            return std::unexpected(from_filesystem_error(std::move(synced.error())));
        }
        auto closed = file->close();
        if (!closed.has_value()) {
            return std::unexpected(from_filesystem_error(std::move(closed.error())));
        }
        return {};
    }

    auto file = filesystem_->open(
        path,
        filesystem::backend::FileOpenOptions {
            .access = filesystem::backend::FileAccess::ReadOnly,
            .create_mode = filesystem::backend::FileCreateMode::OpenExisting,
        }
    );
    if (!file.has_value()) {
        return std::unexpected(from_filesystem_error(std::move(file.error())));
    }

    io::FileByteReader byte_reader {file.value()};
    io::BinaryReader reader {byte_reader};
    auto header_read = read_file_header(reader, ManifestMagic);
    if (!header_read.has_value()) {
        return std::unexpected(std::move(header_read.error()));
    }

    auto version = reader.read_u32();
    if (!version.has_value()) {
        return std::unexpected(from_io_error(std::move(version.error())));
    }
    if (version.value() != DatabaseFormatVersion) {
        return std::unexpected(make_error(ManifestErrorCode::InvalidFormat, "Unsupported manifest storage format version"));
    }

    auto meta_path = reader.read_string();
    if (!meta_path.has_value()) {
        return std::unexpected(from_io_error(std::move(meta_path.error())));
    }
    auto collections_path = reader.read_string();
    if (!collections_path.has_value()) {
        return std::unexpected(from_io_error(std::move(collections_path.error())));
    }
    if (meta_path.value() != MetaFileName || collections_path.value() != CollectionsDirName) {
        return std::unexpected(make_error(ManifestErrorCode::InvalidFormat, "Unsupported manifest paths"));
    }

    return {};
}

const std::filesystem::path & DatabaseManifest::data_dir() const noexcept
{
    return data_dir_;
}

std::filesystem::path DatabaseManifest::meta_path() const
{
    return data_dir_ / MetaFileName;
}

std::filesystem::path DatabaseManifest::collections_dir() const
{
    return data_dir_ / CollectionsDirName;
}

} // namespace litedb::core::database

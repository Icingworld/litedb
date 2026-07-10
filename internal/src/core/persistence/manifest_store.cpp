#include "core/persistence/manifest_store.hpp"

#include <stdexcept>
#include <utility>

#include "core/io/binary_io.hpp"
#include "core/io/file_byte_reader.hpp"
#include "core/io/file_byte_writer.hpp"
#include "core/persistence/storage_format.hpp"

namespace litedb::core::persistence
{

namespace
{

storage::StorageError make_error(storage::StorageErrorCode code, std::string message)
{
    return storage::StorageError {code, std::move(message)};
}

storage::StorageError from_filesystem_error(filesystem::FileSystemError error)
{
    return storage::StorageError {
        .code = storage::StorageErrorCode::IoError,
        .message = std::move(error.message),
    };
}

void require_io(std::expected<void, io::IoError> result)
{
    if (!result.has_value()) {
        throw std::runtime_error(result.error().message);
    }
}

template <typename T>
T require_io(std::expected<T, io::IoError> result)
{
    if (!result.has_value()) {
        throw std::runtime_error(result.error().message);
    }
    return std::move(result.value());
}

void write_file_header(io::BinaryWriter & writer, std::uint32_t magic)
{
    require_io(writer.write_u32(magic));
    require_io(writer.write_u16(StorageFormatVersion));
    require_io(writer.write_u16(FileHeaderSize));
}

void read_file_header(io::BinaryReader & reader, std::uint32_t expected_magic)
{
    if (require_io(reader.read_u32()) != expected_magic) {
        throw std::runtime_error("invalid file magic");
    }
    if (require_io(reader.read_u16()) != StorageFormatVersion) {
        throw std::runtime_error("unsupported storage format version");
    }
    if (require_io(reader.read_u16()) < FileHeaderSize) {
        throw std::runtime_error("invalid file header size");
    }
}

} // namespace

ManifestStore::ManifestStore(std::filesystem::path data_dir, filesystem::FileSystem & filesystem)
    : data_dir_(std::move(data_dir))
    , filesystem_(&filesystem)
{
}

std::expected<void, storage::StorageError> ManifestStore::ensure_initialized() const
{
    try {
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
            write_file_header(writer, ManifestMagic);
            require_io(writer.write_u32(StorageFormatVersion));
            require_io(writer.write_string(CatalogFileName));
            require_io(writer.write_string(CollectionsDirName));
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
        read_file_header(reader, ManifestMagic);
        if (require_io(reader.read_u32()) != StorageFormatVersion) {
            return std::unexpected(make_error(storage::StorageErrorCode::InvalidStorageFormat, "Unsupported manifest storage format version"));
        }
        if (require_io(reader.read_string()) != CatalogFileName || require_io(reader.read_string()) != CollectionsDirName) {
            return std::unexpected(make_error(storage::StorageErrorCode::InvalidStorageFormat, "Unsupported manifest paths"));
        }
        return {};
    } catch (const std::exception & exception) {
        return std::unexpected(make_error(storage::StorageErrorCode::InvalidStorageFormat, exception.what()));
    }
}

const std::filesystem::path & ManifestStore::data_dir() const noexcept
{
    return data_dir_;
}

std::filesystem::path ManifestStore::catalog_path() const
{
    return data_dir_ / CatalogFileName;
}

std::filesystem::path ManifestStore::collections_dir() const
{
    return data_dir_ / CollectionsDirName;
}

} // namespace litedb::core::persistence

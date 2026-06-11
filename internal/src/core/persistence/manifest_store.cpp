#include "core/persistence/manifest_store.hpp"

#include <fstream>
#include <stdexcept>
#include <utility>

#include "core/persistence/binary_io.hpp"
#include "core/persistence/storage_format.hpp"

namespace litedb::core::persistence
{

namespace
{

storage::StorageError make_error(storage::StorageErrorCode code, std::string message)
{
    return storage::StorageError {code, std::move(message)};
}

void write_file_header(BinaryWriter & writer, std::uint32_t magic)
{
    writer.write_u32(magic);
    writer.write_u16(StorageFormatVersion);
    writer.write_u16(FileHeaderSize);
}

void read_file_header(BinaryReader & reader, std::uint32_t expected_magic)
{
    if (reader.read_u32() != expected_magic) {
        throw std::runtime_error("invalid file magic");
    }
    if (reader.read_u16() != StorageFormatVersion) {
        throw std::runtime_error("unsupported storage format version");
    }
    if (reader.read_u16() < FileHeaderSize) {
        throw std::runtime_error("invalid file header size");
    }
}

} // namespace

ManifestStore::ManifestStore(std::filesystem::path data_dir)
    : data_dir_(std::move(data_dir))
{
}

std::expected<void, storage::StorageError> ManifestStore::ensure_initialized() const
{
    try {
        std::filesystem::create_directories(collections_dir());

        const auto path = data_dir_ / ManifestFileName;
        if (!std::filesystem::exists(path)) {
            std::ofstream out {path, std::ios::binary | std::ios::trunc};
            if (!out) {
                return std::unexpected(make_error(storage::StorageErrorCode::IoError, "Failed to create manifest file"));
            }
            BinaryWriter writer {out};
            write_file_header(writer, ManifestMagic);
            writer.write_u32(StorageFormatVersion);
            writer.write_string(CatalogFileName);
            writer.write_string(CollectionsDirName);
            out.flush();
            if (!out) {
                return std::unexpected(make_error(storage::StorageErrorCode::IoError, "Failed to flush manifest file"));
            }
            return {};
        }

        std::ifstream in {path, std::ios::binary};
        if (!in) {
            return std::unexpected(make_error(storage::StorageErrorCode::IoError, "Failed to open manifest file"));
        }
        BinaryReader reader {in};
        read_file_header(reader, ManifestMagic);
        if (reader.read_u32() != StorageFormatVersion) {
            return std::unexpected(make_error(storage::StorageErrorCode::InvalidStorageFormat, "Unsupported manifest storage format version"));
        }
        if (reader.read_string() != CatalogFileName || reader.read_string() != CollectionsDirName) {
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

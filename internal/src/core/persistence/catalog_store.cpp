#include "core/persistence/catalog_store.hpp"

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

void write_optional_u64(io::BinaryWriter & writer, const std::optional<std::size_t> & value)
{
    require_io(writer.write_u8(value.has_value() ? 1U : 0U));
    if (value.has_value()) {
        require_io(writer.write_u64(static_cast<std::uint64_t>(value.value())));
    }
}

std::optional<std::size_t> read_optional_u64(io::BinaryReader & reader)
{
    if (require_io(reader.read_u8()) == 0) {
        return std::nullopt;
    }
    return static_cast<std::size_t>(require_io(reader.read_u64()));
}

void write_default_expression(io::BinaryWriter & writer, const catalog::CatalogDefaultExpression & expression)
{
    require_io(writer.write_u8(static_cast<std::uint8_t>(expression.kind)));
    require_io(writer.write_u8(static_cast<std::uint8_t>(expression.literal_kind)));
    require_io(writer.write_string(expression.value));
    require_io(writer.write_u32(static_cast<std::uint32_t>(expression.elements.size())));
    for (const auto & element : expression.elements) {
        write_default_expression(writer, element);
    }
}

catalog::CatalogDefaultExpression read_default_expression(io::BinaryReader & reader)
{
    catalog::CatalogDefaultExpression expression;
    expression.kind = static_cast<catalog::CatalogDefaultExpressionKind>(require_io(reader.read_u8()));
    expression.literal_kind = static_cast<catalog::CatalogDefaultLiteralKind>(require_io(reader.read_u8()));
    expression.value = require_io(reader.read_string());
    const auto count = require_io(reader.read_u32());
    expression.elements.reserve(count);
    for (std::uint32_t index = 0; index < count; ++index) {
        expression.elements.push_back(read_default_expression(reader));
    }
    return expression;
}

void write_optional_default_expression(
    io::BinaryWriter & writer,
    const std::optional<catalog::CatalogDefaultExpression> & expression
)
{
    require_io(writer.write_u8(expression.has_value() ? 1U : 0U));
    if (expression.has_value()) {
        write_default_expression(writer, expression.value());
    }
}

std::optional<catalog::CatalogDefaultExpression> read_optional_default_expression(io::BinaryReader & reader)
{
    if (require_io(reader.read_u8()) == 0) {
        return std::nullopt;
    }
    return read_default_expression(reader);
}

void write_optional_string(io::BinaryWriter & writer, const std::optional<std::string> & value)
{
    require_io(writer.write_u8(value.has_value() ? 1U : 0U));
    if (value.has_value()) {
        require_io(writer.write_string(value.value()));
    }
}

std::optional<std::string> read_optional_string(io::BinaryReader & reader)
{
    if (require_io(reader.read_u8()) == 0) {
        return std::nullopt;
    }
    return require_io(reader.read_string());
}

void write_snapshot(io::BinaryWriter & writer, const catalog::CatalogSnapshot & snapshot)
{
    write_file_header(writer, CatalogMagic);
    require_io(writer.write_u64(snapshot.next_database_id));
    require_io(writer.write_u64(snapshot.next_collection_id));
    require_io(writer.write_u64(snapshot.next_column_id));
    require_io(writer.write_u64(snapshot.next_index_id));
    require_io(writer.write_u64(snapshot.next_vector_index_id));
    require_io(writer.write_u32(static_cast<std::uint32_t>(snapshot.databases.size())));

    for (const auto & database : snapshot.databases) {
        require_io(writer.write_u64(database.id));
        require_io(writer.write_string(database.name));
        require_io(writer.write_u32(static_cast<std::uint32_t>(database.collections.size())));
        for (const auto & collection : database.collections) {
            require_io(writer.write_u64(collection.id));
            require_io(writer.write_string(collection.name));
            write_optional_string(writer, collection.comment);
            require_io(writer.write_u32(static_cast<std::uint32_t>(collection.columns.size())));
            for (const auto & column : collection.columns) {
                require_io(writer.write_u64(column.id));
                require_io(writer.write_string(column.name));
                require_io(writer.write_u8(static_cast<std::uint8_t>(column.type.id)));
                write_optional_u64(writer, column.type.parameter);
                require_io(writer.write_u8(column.nullable ? 1U : 0U));
                require_io(writer.write_u8(column.primary_key ? 1U : 0U));
                require_io(writer.write_u8(column.unique ? 1U : 0U));
                write_optional_default_expression(writer, column.default_expression);
                write_optional_string(writer, column.comment);
            }
            require_io(writer.write_u32(static_cast<std::uint32_t>(collection.indexes.size())));
            for (const auto & index : collection.indexes) {
                require_io(writer.write_u64(index.id));
                require_io(writer.write_u64(index.column_id));
                require_io(writer.write_string(index.name));
                require_io(writer.write_u8(static_cast<std::uint8_t>(index.index_kind)));
                require_io(writer.write_u8(index.unique ? 1U : 0U));
            }
            require_io(writer.write_u32(static_cast<std::uint32_t>(collection.vector_indexes.size())));
            for (const auto & index : collection.vector_indexes) {
                require_io(writer.write_u64(index.id));
                require_io(writer.write_u64(index.column_id));
                require_io(writer.write_string(index.name));
                require_io(writer.write_u8(static_cast<std::uint8_t>(index.index_kind)));
                require_io(writer.write_u8(static_cast<std::uint8_t>(index.metric)));
                require_io(writer.write_u64(index.dimension));
                require_io(writer.write_u64(index.max_neighbors));
                require_io(writer.write_u64(index.ef_construction));
                require_io(writer.write_u64(index.ef_search_default));
                require_io(writer.write_u64(index.random_seed));
            }
        }
    }
}

catalog::CatalogSnapshot read_snapshot(io::BinaryReader & reader)
{
    read_file_header(reader, CatalogMagic);

    catalog::CatalogSnapshot snapshot;
    snapshot.next_database_id = require_io(reader.read_u64());
    snapshot.next_collection_id = require_io(reader.read_u64());
    snapshot.next_column_id = require_io(reader.read_u64());
    snapshot.next_index_id = require_io(reader.read_u64());
    snapshot.next_vector_index_id = require_io(reader.read_u64());

    const auto database_count = require_io(reader.read_u32());
    snapshot.databases.reserve(database_count);
    for (std::uint32_t database_index = 0; database_index < database_count; ++database_index) {
        catalog::CatalogSnapshotDatabase database;
        database.id = require_io(reader.read_u64());
        database.name = require_io(reader.read_string());

        const auto collection_count = require_io(reader.read_u32());
        database.collections.reserve(collection_count);
        for (std::uint32_t collection_index = 0; collection_index < collection_count; ++collection_index) {
            catalog::CatalogSnapshotCollection collection;
            collection.id = require_io(reader.read_u64());
            collection.database_id = database.id;
            collection.name = require_io(reader.read_string());
            collection.comment = read_optional_string(reader);

            const auto column_count = require_io(reader.read_u32());
            collection.columns.reserve(column_count);
            for (std::uint32_t column_index = 0; column_index < column_count; ++column_index) {
                catalog::CatalogSnapshotColumn column;
                column.id = require_io(reader.read_u64());
                column.name = require_io(reader.read_string());
                column.type.id = static_cast<common::LogicalTypeId>(require_io(reader.read_u8()));
                column.type.parameter = read_optional_u64(reader);
                column.nullable = require_io(reader.read_u8()) != 0;
                column.primary_key = require_io(reader.read_u8()) != 0;
                column.unique = require_io(reader.read_u8()) != 0;
                column.default_expression = read_optional_default_expression(reader);
                column.comment = read_optional_string(reader);
                collection.columns.push_back(std::move(column));
            }

            const auto index_count = require_io(reader.read_u32());
            collection.indexes.reserve(index_count);
            for (std::uint32_t index_index = 0; index_index < index_count; ++index_index) {
                catalog::CatalogSnapshotIndex index;
                index.id = require_io(reader.read_u64());
                index.column_id = require_io(reader.read_u64());
                index.name = require_io(reader.read_string());
                index.index_kind = static_cast<catalog::CatalogIndexKind>(require_io(reader.read_u8()));
                index.unique = require_io(reader.read_u8()) != 0;
                collection.indexes.push_back(std::move(index));
            }

            const auto vector_index_count = require_io(reader.read_u32());
            collection.vector_indexes.reserve(vector_index_count);
            for (std::uint32_t index_index = 0; index_index < vector_index_count; ++index_index) {
                catalog::CatalogSnapshotVectorIndex index;
                index.id = require_io(reader.read_u64());
                index.column_id = require_io(reader.read_u64());
                index.name = require_io(reader.read_string());
                index.index_kind = static_cast<catalog::CatalogVectorIndexKind>(require_io(reader.read_u8()));
                index.metric = static_cast<catalog::CatalogVectorDistanceMetric>(require_io(reader.read_u8()));
                index.dimension = static_cast<std::size_t>(require_io(reader.read_u64()));
                index.max_neighbors = static_cast<std::size_t>(require_io(reader.read_u64()));
                index.ef_construction = static_cast<std::size_t>(require_io(reader.read_u64()));
                index.ef_search_default = static_cast<std::size_t>(require_io(reader.read_u64()));
                index.random_seed = static_cast<std::size_t>(require_io(reader.read_u64()));
                collection.vector_indexes.push_back(std::move(index));
            }

            database.collections.push_back(std::move(collection));
        }

        snapshot.databases.push_back(std::move(database));
    }
    return snapshot;
}

} // namespace

CatalogStore::CatalogStore(std::filesystem::path path, filesystem::FileSystem & filesystem)
    : path_(std::move(path))
    , filesystem_(&filesystem)
{
}

std::expected<catalog::CatalogSnapshot, storage::StorageError> CatalogStore::load_or_empty() const
{
    try {
        auto exists = filesystem_->exists(path_);
        if (!exists.has_value()) {
            return std::unexpected(from_filesystem_error(std::move(exists.error())));
        }
        if (!exists.value()) {
            return catalog::CatalogSnapshot {};
        }

        auto file = filesystem_->open(
            path_,
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
        return read_snapshot(reader);
    } catch (const std::exception & exception) {
        return std::unexpected(make_error(storage::StorageErrorCode::InvalidStorageFormat, exception.what()));
    }
}

std::expected<void, storage::StorageError> CatalogStore::save(const catalog::CatalogSnapshot & snapshot) const
{
    try {
        auto created_dir = filesystem_->create_dir_all(path_.parent_path());
        if (!created_dir.has_value()) {
            return std::unexpected(from_filesystem_error(std::move(created_dir.error())));
        }
        const auto temp_path = path_.string() + ".tmp";
        {
            auto file = filesystem_->open(
                temp_path,
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
            write_snapshot(writer, snapshot);
            auto synced = file->sync_all();
            if (!synced.has_value()) {
                return std::unexpected(from_filesystem_error(std::move(synced.error())));
            }
            auto closed = file->close();
            if (!closed.has_value()) {
                return std::unexpected(from_filesystem_error(std::move(closed.error())));
            }
        }

        auto exists = filesystem_->exists(path_);
        if (!exists.has_value()) {
            return std::unexpected(from_filesystem_error(std::move(exists.error())));
        }
        if (exists.value()) {
            auto removed = filesystem_->remove(path_);
            if (!removed.has_value()) {
                return std::unexpected(from_filesystem_error(std::move(removed.error())));
            }
        }
        auto renamed = filesystem_->rename(temp_path, path_);
        if (!renamed.has_value()) {
            return std::unexpected(from_filesystem_error(std::move(renamed.error())));
        }
        return {};
    } catch (const std::exception & exception) {
        return std::unexpected(make_error(storage::StorageErrorCode::IoError, exception.what()));
    }
}

} // namespace litedb::core::persistence

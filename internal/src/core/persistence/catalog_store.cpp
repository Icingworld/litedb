#include "core/persistence/catalog_store.hpp"

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

void write_optional_u64(BinaryWriter & writer, const std::optional<std::size_t> & value)
{
    writer.write_u8(value.has_value() ? 1U : 0U);
    if (value.has_value()) {
        writer.write_u64(static_cast<std::uint64_t>(value.value()));
    }
}

std::optional<std::size_t> read_optional_u64(BinaryReader & reader)
{
    if (reader.read_u8() == 0) {
        return std::nullopt;
    }
    return static_cast<std::size_t>(reader.read_u64());
}

void write_default_expression(BinaryWriter & writer, const catalog::CatalogDefaultExpression & expression)
{
    writer.write_u8(static_cast<std::uint8_t>(expression.kind));
    writer.write_u8(static_cast<std::uint8_t>(expression.literal_kind));
    writer.write_string(expression.value);
    writer.write_u32(static_cast<std::uint32_t>(expression.elements.size()));
    for (const auto & element : expression.elements) {
        write_default_expression(writer, element);
    }
}

catalog::CatalogDefaultExpression read_default_expression(BinaryReader & reader)
{
    catalog::CatalogDefaultExpression expression;
    expression.kind = static_cast<catalog::CatalogDefaultExpressionKind>(reader.read_u8());
    expression.literal_kind = static_cast<catalog::CatalogDefaultLiteralKind>(reader.read_u8());
    expression.value = reader.read_string();
    const auto count = reader.read_u32();
    expression.elements.reserve(count);
    for (std::uint32_t index = 0; index < count; ++index) {
        expression.elements.push_back(read_default_expression(reader));
    }
    return expression;
}

void write_optional_default_expression(
    BinaryWriter & writer,
    const std::optional<catalog::CatalogDefaultExpression> & expression
)
{
    writer.write_u8(expression.has_value() ? 1U : 0U);
    if (expression.has_value()) {
        write_default_expression(writer, expression.value());
    }
}

std::optional<catalog::CatalogDefaultExpression> read_optional_default_expression(BinaryReader & reader)
{
    if (reader.read_u8() == 0) {
        return std::nullopt;
    }
    return read_default_expression(reader);
}

void write_optional_string(BinaryWriter & writer, const std::optional<std::string> & value)
{
    writer.write_u8(value.has_value() ? 1U : 0U);
    if (value.has_value()) {
        writer.write_string(value.value());
    }
}

std::optional<std::string> read_optional_string(BinaryReader & reader)
{
    if (reader.read_u8() == 0) {
        return std::nullopt;
    }
    return reader.read_string();
}

void write_snapshot(BinaryWriter & writer, const catalog::CatalogSnapshot & snapshot)
{
    write_file_header(writer, CatalogMagic);
    writer.write_u64(snapshot.next_database_id);
    writer.write_u64(snapshot.next_collection_id);
    writer.write_u64(snapshot.next_column_id);
    writer.write_u64(snapshot.next_index_id);
    writer.write_u64(snapshot.next_vector_index_id);
    writer.write_u32(static_cast<std::uint32_t>(snapshot.databases.size()));

    for (const auto & database : snapshot.databases) {
        writer.write_u64(database.id);
        writer.write_string(database.name);
        writer.write_u32(static_cast<std::uint32_t>(database.collections.size()));
        for (const auto & collection : database.collections) {
            writer.write_u64(collection.id);
            writer.write_string(collection.name);
            write_optional_string(writer, collection.comment);
            writer.write_u32(static_cast<std::uint32_t>(collection.columns.size()));
            for (const auto & column : collection.columns) {
                writer.write_u64(column.id);
                writer.write_string(column.name);
                writer.write_u8(static_cast<std::uint8_t>(column.type.id));
                write_optional_u64(writer, column.type.parameter);
                writer.write_u8(column.nullable ? 1U : 0U);
                writer.write_u8(column.primary_key ? 1U : 0U);
                writer.write_u8(column.unique ? 1U : 0U);
                write_optional_default_expression(writer, column.default_expression);
                write_optional_string(writer, column.comment);
            }
            writer.write_u32(static_cast<std::uint32_t>(collection.indexes.size()));
            for (const auto & index : collection.indexes) {
                writer.write_u64(index.id);
                writer.write_u64(index.column_id);
                writer.write_string(index.name);
                writer.write_u8(static_cast<std::uint8_t>(index.index_kind));
                writer.write_u8(index.unique ? 1U : 0U);
            }
            writer.write_u32(static_cast<std::uint32_t>(collection.vector_indexes.size()));
            for (const auto & index : collection.vector_indexes) {
                writer.write_u64(index.id);
                writer.write_u64(index.column_id);
                writer.write_string(index.name);
                writer.write_u8(static_cast<std::uint8_t>(index.index_kind));
                writer.write_u8(static_cast<std::uint8_t>(index.metric));
                writer.write_u64(index.dimension);
                writer.write_u64(index.max_neighbors);
                writer.write_u64(index.ef_construction);
                writer.write_u64(index.ef_search_default);
                writer.write_u64(index.random_seed);
            }
        }
    }
}

catalog::CatalogSnapshot read_snapshot(BinaryReader & reader)
{
    read_file_header(reader, CatalogMagic);

    catalog::CatalogSnapshot snapshot;
    snapshot.next_database_id = reader.read_u64();
    snapshot.next_collection_id = reader.read_u64();
    snapshot.next_column_id = reader.read_u64();
    snapshot.next_index_id = reader.read_u64();
    snapshot.next_vector_index_id = reader.read_u64();

    const auto database_count = reader.read_u32();
    snapshot.databases.reserve(database_count);
    for (std::uint32_t database_index = 0; database_index < database_count; ++database_index) {
        catalog::CatalogSnapshotDatabase database;
        database.id = reader.read_u64();
        database.name = reader.read_string();

        const auto collection_count = reader.read_u32();
        database.collections.reserve(collection_count);
        for (std::uint32_t collection_index = 0; collection_index < collection_count; ++collection_index) {
            catalog::CatalogSnapshotCollection collection;
            collection.id = reader.read_u64();
            collection.database_id = database.id;
            collection.name = reader.read_string();
            collection.comment = read_optional_string(reader);

            const auto column_count = reader.read_u32();
            collection.columns.reserve(column_count);
            for (std::uint32_t column_index = 0; column_index < column_count; ++column_index) {
                catalog::CatalogSnapshotColumn column;
                column.id = reader.read_u64();
                column.name = reader.read_string();
                column.type.id = static_cast<common::LogicalTypeId>(reader.read_u8());
                column.type.parameter = read_optional_u64(reader);
                column.nullable = reader.read_u8() != 0;
                column.primary_key = reader.read_u8() != 0;
                column.unique = reader.read_u8() != 0;
                column.default_expression = read_optional_default_expression(reader);
                column.comment = read_optional_string(reader);
                collection.columns.push_back(std::move(column));
            }

            const auto index_count = reader.read_u32();
            collection.indexes.reserve(index_count);
            for (std::uint32_t index_index = 0; index_index < index_count; ++index_index) {
                catalog::CatalogSnapshotIndex index;
                index.id = reader.read_u64();
                index.column_id = reader.read_u64();
                index.name = reader.read_string();
                index.index_kind = static_cast<catalog::CatalogIndexKind>(reader.read_u8());
                index.unique = reader.read_u8() != 0;
                collection.indexes.push_back(std::move(index));
            }

            const auto vector_index_count = reader.read_u32();
            collection.vector_indexes.reserve(vector_index_count);
            for (std::uint32_t index_index = 0; index_index < vector_index_count; ++index_index) {
                catalog::CatalogSnapshotVectorIndex index;
                index.id = reader.read_u64();
                index.column_id = reader.read_u64();
                index.name = reader.read_string();
                index.index_kind = static_cast<catalog::CatalogVectorIndexKind>(reader.read_u8());
                index.metric = static_cast<catalog::CatalogVectorDistanceMetric>(reader.read_u8());
                index.dimension = static_cast<std::size_t>(reader.read_u64());
                index.max_neighbors = static_cast<std::size_t>(reader.read_u64());
                index.ef_construction = static_cast<std::size_t>(reader.read_u64());
                index.ef_search_default = static_cast<std::size_t>(reader.read_u64());
                index.random_seed = static_cast<std::size_t>(reader.read_u64());
                collection.vector_indexes.push_back(std::move(index));
            }

            database.collections.push_back(std::move(collection));
        }

        snapshot.databases.push_back(std::move(database));
    }
    return snapshot;
}

} // namespace

CatalogStore::CatalogStore(std::filesystem::path path)
    : path_(std::move(path))
{
}

std::expected<catalog::CatalogSnapshot, storage::StorageError> CatalogStore::load_or_empty() const
{
    try {
        if (!std::filesystem::exists(path_)) {
            return catalog::CatalogSnapshot {};
        }

        std::ifstream in {path_, std::ios::binary};
        if (!in) {
            return std::unexpected(make_error(storage::StorageErrorCode::IoError, "Failed to open catalog file"));
        }
        BinaryReader reader {in};
        return read_snapshot(reader);
    } catch (const std::exception & exception) {
        return std::unexpected(make_error(storage::StorageErrorCode::InvalidStorageFormat, exception.what()));
    }
}

std::expected<void, storage::StorageError> CatalogStore::save(const catalog::CatalogSnapshot & snapshot) const
{
    try {
        std::filesystem::create_directories(path_.parent_path());
        const auto temp_path = path_.string() + ".tmp";
        {
            std::ofstream out {temp_path, std::ios::binary | std::ios::trunc};
            if (!out) {
                return std::unexpected(make_error(storage::StorageErrorCode::IoError, "Failed to create catalog temp file"));
            }
            BinaryWriter writer {out};
            write_snapshot(writer, snapshot);
            out.flush();
            if (!out) {
                return std::unexpected(make_error(storage::StorageErrorCode::IoError, "Failed to flush catalog file"));
            }
        }

        std::error_code ignored;
        std::filesystem::remove(path_, ignored);
        std::filesystem::rename(temp_path, path_);
        return {};
    } catch (const std::exception & exception) {
        return std::unexpected(make_error(storage::StorageErrorCode::IoError, exception.what()));
    }
}

} // namespace litedb::core::persistence

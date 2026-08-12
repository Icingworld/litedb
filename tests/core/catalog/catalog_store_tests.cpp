#include "core/filesystem/platform_filesystem.hpp"
#include "core/filesystem/backend/filesystem_backend.hpp"
#include "core/io/binary_io.hpp"
#include "core/io/buffer_byte_writer.hpp"
#include "core/io/checksum.hpp"
#include "core/io/file_byte_writer.hpp"
#include "core/catalog/catalog_store.hpp"

#include <chrono>
#include <array>
#include <exception>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <limits>
#include <stdexcept>

namespace
{

using namespace litedb::core;

void require(bool condition, const char * message)
{
    if (!condition) {
        throw std::runtime_error(message);
    }
}

catalog::CatalogSnapshot make_snapshot()
{
    catalog::CatalogSnapshot snapshot;
    snapshot.next_database_id = 2;
    snapshot.next_collection_id = 3;
    snapshot.next_column_id = 5;
    snapshot.next_index_id = 7;
    snapshot.next_vector_index_id = 9;

    catalog::CatalogColumnSnapshot id;
    id.id = 3;
    id.name = "id";
    id.type = {common::LogicalTypeId::BigInt, std::nullopt};
    id.unique = true;
    id.nullable = false;
    id.comment = "identifier";

    catalog::CatalogColumnSnapshot embedding;
    embedding.id = 4;
    embedding.name = "embedding";
    embedding.type = {common::LogicalTypeId::Vector, 3};
    embedding.default_expression = schema::DefaultExpression::vector({
        schema::DefaultExpression::literal(schema::DefaultLiteralKind::Float, "0.0"),
        schema::DefaultExpression::literal(schema::DefaultLiteralKind::Float, "0.0"),
        schema::DefaultExpression::literal(schema::DefaultLiteralKind::Float, "0.0"),
    });

    catalog::CatalogCollectionSnapshot collection;
    collection.id = 2;
    collection.database_id = 1;
    collection.name = "items";
    collection.comment = "test collection";
    collection.columns = {id, embedding};
    collection.indexes.push_back({6, 3, "idx_items", catalog::entry::IndexKind::BTree, true});
    collection.vector_indexes.push_back({
        8, 4, "vidx_embedding", catalog::entry::VectorIndexKind::Hnsw,
        catalog::entry::VectorDistanceMetric::Cosine, 3, 24, 240, 80, 7,
    });

    catalog::CatalogDatabaseSnapshot database;
    database.id = 1;
    database.name = "main";
    database.collections.push_back(std::move(collection));
    snapshot.databases.push_back(std::move(database));
    return snapshot;
}

void test_missing_file_and_roundtrip(const std::filesystem::path & path)
{
    auto filesystem = filesystem::create_platform_filesystem();
    catalog::CatalogStore store {path, filesystem};
    const auto empty = store.load();
    require(empty.has_value(), "missing catalog file should return an empty snapshot");
    require(!empty->has_value(), "missing file must not be confused with an empty catalog");

    const auto snapshot = make_snapshot();
    require(store.save(snapshot).has_value(), "save catalog snapshot failed");
    {
        std::ifstream input {path, std::ios::binary};
        std::array<unsigned char, 24> header {};
        input.read(reinterpret_cast<char *>(header.data()), static_cast<std::streamsize>(header.size()));
        require(input.gcount() == static_cast<std::streamsize>(header.size()), "V2 header is truncated");
        require(header[0] == 0x4c && header[1] == 0x44 && header[2] == 0x4d && header[3] == 0x54,
                "V2 magic bytes mismatch");
        require(header[4] == 2 && header[5] == 0 && header[6] == 24 && header[7] == 0,
                "V2 version or header-size bytes mismatch");
        require(header[20] == 0 && header[21] == 0 && header[22] == 0 && header[23] == 0,
                "V2 reserved flags must be zero");
    }
    const auto loaded = store.load();
    require(loaded.has_value(), "load catalog snapshot failed");
    require(loaded->has_value(), "saved snapshot was reported missing");
    require((**loaded).next_vector_index_id == 9, "next id roundtrip mismatch");
    require((**loaded).databases.size() == 1, "database count mismatch");
    const auto & collection = (**loaded).databases[0].collections[0];
    require(collection.database_id == 1, "collection database id mismatch");
    require(collection.columns[1].type.parameter == 3, "logical type parameter mismatch");
    require(collection.columns[1].default_expression->elements.size() == 3, "default expression mismatch");
    require(collection.indexes[0].column_id == 3, "scalar index column mismatch");
    require(collection.vector_indexes[0].metric == catalog::entry::VectorDistanceMetric::Cosine,
            "vector metric mismatch");

    auto replacement = snapshot;
    replacement.next_vector_index_id = 42;
    require(store.save(replacement).has_value(), "replace existing catalog snapshot failed");
    const auto replaced = store.load();
    require(replaced.has_value(), "load replaced catalog snapshot failed");
    require(replaced->has_value() && (**replaced).next_vector_index_id == 42,
            "catalog snapshot replacement did not publish");
}

void test_checksum_and_trailing_data_rejected(const std::filesystem::path & path)
{
    auto filesystem = filesystem::create_platform_filesystem();
    catalog::CatalogStore store {path, filesystem};
    require(store.save(make_snapshot()).has_value(), "save checksum fixture failed");
    {
        std::fstream file {path, std::ios::binary | std::ios::in | std::ios::out};
        file.seekg(24);
        char byte {};
        file.read(&byte, 1);
        byte ^= 0x01;
        file.seekp(24);
        file.write(&byte, 1);
    }
    auto corrupted = store.load();
    require(!corrupted && corrupted.error().is(catalog::CatalogErrorCode::ChecksumMismatch),
            "payload corruption must report checksum mismatch");

    require(store.save(make_snapshot()).has_value(), "restore trailing-data fixture failed");
    {
        std::ofstream file {path, std::ios::binary | std::ios::app};
        file.put('\0');
    }
    auto trailing = store.load();
    require(!trailing && trailing.error().is(catalog::CatalogErrorCode::InvalidFormat),
            "trailing data must be rejected");
}

void write_test_header(
    filesystem::FileSystem & filesystem,
    const std::filesystem::path & path,
    std::uint32_t magic,
    std::optional<std::uint16_t> version
)
{
    auto file = filesystem.open(path, {
        .access = filesystem::FileAccess::ReadWrite,
        .create_mode = filesystem::FileCreateMode::CreateOrTruncate,
    });
    require(file.has_value(), "open test catalog file failed");
    io::FileByteWriter byte_writer {*file};
    io::LittleEndianBinaryWriter writer {byte_writer};
    require(writer.write_u32(magic).has_value(), "write test magic failed");
    if (version) {
        require(writer.write_u16(*version).has_value(), "write test version failed");
        require(writer.write_u16(24).has_value(), "write test header size failed");
        require(writer.write_u64(0).has_value(), "write test payload size failed");
        require(writer.write_u32(0).has_value(), "write test checksum failed");
        require(writer.write_u32(0).has_value(), "write test flags failed");
    }
    require(file->close().has_value(), "close test catalog file failed");
}

void test_load_error_codes(const std::filesystem::path & path)
{
    constexpr std::uint32_t catalog_magic = 0x544d444c;
    auto filesystem = filesystem::create_platform_filesystem();
    catalog::CatalogStore store {path, filesystem};

    write_test_header(filesystem, path, 0, 1);
    auto invalid_magic = store.load();
    require(!invalid_magic.has_value(), "invalid magic should fail");
    require(invalid_magic.error().is(catalog::CatalogErrorCode::InvalidFormat),
            "invalid magic error code mismatch");

    write_test_header(filesystem, path, catalog_magic, 1);
    auto unsupported_version = store.load();
    require(!unsupported_version.has_value(), "unsupported version should fail");
    require(unsupported_version.error().is(catalog::CatalogErrorCode::UnsupportedVersion),
            "unsupported version error code mismatch");

    write_test_header(filesystem, path, catalog_magic, std::nullopt);
    auto truncated = store.load();
    require(!truncated.has_value(), "truncated catalog file should fail");
    require(truncated.error().is(catalog::CatalogErrorCode::UnexpectedEof),
            "truncated file error code mismatch");

    auto file = filesystem.open(path, {
        .access = filesystem::FileAccess::ReadWrite,
        .create_mode = filesystem::FileCreateMode::CreateOrTruncate,
    });
    require(file.has_value(), "open malicious catalog file failed");
    io::BufferByteWriter payload_bytes {128};
    io::LittleEndianBinaryWriter payload_writer {payload_bytes};
    for (int index = 0; index < 5; ++index) {
        require(payload_writer.write_u64(1).has_value(), "write malicious id failed");
    }
    require(payload_writer.write_u32(std::numeric_limits<std::uint32_t>::max()).has_value(),
            "write malicious count failed");

    io::FileByteWriter byte_writer {*file};
    io::LittleEndianBinaryWriter writer {byte_writer};
    require(writer.write_u32(catalog_magic).has_value(), "write malicious magic failed");
    require(writer.write_u16(2).has_value(), "write malicious version failed");
    require(writer.write_u16(24).has_value(), "write malicious header failed");
    require(writer.write_u64(payload_bytes.bytes().size()).has_value(), "write malicious payload size failed");
    require(writer.write_u32(io::crc32(payload_bytes.bytes())).has_value(), "write malicious checksum failed");
    require(writer.write_u32(0).has_value(), "write malicious flags failed");
    require(byte_writer.write_bytes(payload_bytes.bytes()).has_value(), "write malicious payload failed");
    require(file->close().has_value(), "close malicious catalog file failed");
    auto oversized_count = store.load();
    require(!oversized_count.has_value(), "oversized metadata count should fail");
    require(oversized_count.error().is(catalog::CatalogErrorCode::ValueTooLarge),
            "oversized metadata count error mismatch");
}

} // namespace

int main()
{
    const auto suffix = std::chrono::steady_clock::now().time_since_epoch().count();
    const auto directory = std::filesystem::temp_directory_path() / ("litedb_catalog_store_" + std::to_string(suffix));
    const auto path = directory / "catalog.lcat";
    try {
        test_missing_file_and_roundtrip(path);
        test_load_error_codes(path);
        test_checksum_and_trailing_data_rejected(path);
        std::filesystem::remove_all(directory);
    } catch (const std::exception & exception) {
        std::filesystem::remove_all(directory);
        std::cerr << exception.what() << '\n';
        return 1;
    }
    return 0;
}

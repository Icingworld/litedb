#include "core/filesystem/platform_filesystem.hpp"
#include "core/filesystem/backend/filesystem_backend.hpp"
#include "core/io/binary_io.hpp"
#include "core/io/buffer_byte_writer.hpp"
#include "core/io/checksum.hpp"
#include "core/io/file_byte_writer.hpp"
#include "core/catalog/catalog_constant.hpp"
#include "core/catalog/catalog_store.hpp"

#include <chrono>
#include <array>
#include <cstddef>
#include <exception>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <limits>
#include <span>
#include <stdexcept>
#include <string>
#include <vector>

namespace
{

using namespace litedb::core;

void require(bool condition, const char * message)
{
    if (!condition) {
        throw std::runtime_error(message);
    }
}

void write_catalog_payload(
    filesystem::FileSystem & filesystem,
    const std::filesystem::path & path,
    std::span<const std::byte> payload
)
{
    auto file = filesystem.open(path, {
        .access = filesystem::FileAccess::ReadWrite,
        .create_mode = filesystem::FileCreateMode::CreateOrTruncate,
    });
    require(file.has_value(), "open catalog payload fixture failed");

    io::FileByteWriter byte_writer {*file};
    io::LittleEndianBinaryWriter writer {byte_writer};
    require(writer.write_u32(catalog::CatalogMagic).has_value(), "write catalog fixture magic failed");
    require(writer.write_u16(catalog::CatalogVersion).has_value(), "write catalog fixture version failed");
    require(writer.write_u16(catalog::CatalogHeaderSize).has_value(), "write catalog fixture header size failed");
    require(writer.write_u64(payload.size()).has_value(), "write catalog fixture payload size failed");
    require(writer.write_u32(io::crc32(payload)).has_value(), "write catalog fixture checksum failed");
    require(writer.write_u32(0).has_value(), "write catalog fixture flags failed");
    require(byte_writer.write_bytes(payload).has_value(), "write catalog fixture payload failed");
    require(file->close().has_value(), "close catalog payload fixture failed");
}

std::vector<std::byte> read_catalog_payload(const std::filesystem::path & path)
{
    std::ifstream input {path, std::ios::binary | std::ios::ate};
    require(input.is_open(), "open catalog payload for reading failed");
    const auto file_size = static_cast<std::streamoff>(input.tellg());
    require(file_size >= static_cast<std::streamoff>(catalog::CatalogHeaderSize),
            "catalog payload fixture header is truncated");

    const auto payload_size = static_cast<std::size_t>(
        file_size - static_cast<std::streamoff>(catalog::CatalogHeaderSize)
    );
    std::vector<std::byte> payload(payload_size);
    input.seekg(static_cast<std::streamoff>(catalog::CatalogHeaderSize), std::ios::beg);
    input.read(reinterpret_cast<char *>(payload.data()), static_cast<std::streamsize>(payload.size()));
    require(input.gcount() == static_cast<std::streamsize>(payload.size()),
            "read catalog payload fixture failed");
    return payload;
}

void write_snapshot_id_prefix(io::LittleEndianBinaryWriter & writer)
{
    for (int index = 0; index < 5; ++index) {
        require(writer.write_u64(1).has_value(), "write catalog snapshot id prefix failed");
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
        require(input.gcount() == static_cast<std::streamsize>(header.size()), "V1 header is truncated");
        require(header[0] == 0x4c && header[1] == 0x44 && header[2] == 0x4d && header[3] == 0x54,
                "V1 magic bytes mismatch");
        require(header[4] == 1 && header[5] == 0 && header[6] == 24 && header[7] == 0,
                "V1 version or header-size bytes mismatch");
        require(header[20] == 0 && header[21] == 0 && header[22] == 0 && header[23] == 0,
                "V1 reserved flags must be zero");
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
    auto file_trailing = store.load();
    require(!file_trailing && file_trailing.error().is(catalog::CatalogErrorCode::InvalidFormat),
            "file-level trailing data must be rejected");

    require(store.save(make_snapshot()).has_value(), "restore payload trailing-data fixture failed");
    auto payload = read_catalog_payload(path);
    payload.push_back(std::byte {0});
    write_catalog_payload(filesystem, path, payload);
    auto payload_trailing = store.load();
    require(!payload_trailing && payload_trailing.error().is(catalog::CatalogErrorCode::InvalidFormat),
            "CRC-valid payload trailing data must be rejected");
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
        require(writer.write_u16(catalog::CatalogHeaderSize).has_value(), "write test header size failed");
        require(writer.write_u64(0).has_value(), "write test payload size failed");
        require(writer.write_u32(0).has_value(), "write test checksum failed");
        require(writer.write_u32(0).has_value(), "write test flags failed");
    }
    require(file->close().has_value(), "close test catalog file failed");
}

void test_load_error_codes(const std::filesystem::path & path)
{
    auto filesystem = filesystem::create_platform_filesystem();
    catalog::CatalogStore store {path, filesystem};

    write_test_header(filesystem, path, 0, catalog::CatalogVersion);
    auto invalid_magic = store.load();
    require(!invalid_magic.has_value(), "invalid magic should fail");
    require(invalid_magic.error().is(catalog::CatalogErrorCode::InvalidFormat),
            "invalid magic error code mismatch");

    write_test_header(filesystem, path, catalog::CatalogMagic, catalog::CatalogVersion + 1);
    auto unsupported_version = store.load();
    require(!unsupported_version.has_value(), "unsupported version should fail");
    require(unsupported_version.error().is(catalog::CatalogErrorCode::UnsupportedVersion),
            "unsupported version error code mismatch");

    write_test_header(filesystem, path, catalog::CatalogMagic, std::nullopt);
    auto truncated = store.load();
    require(!truncated.has_value(), "truncated catalog file should fail");
    require(truncated.error().is(catalog::CatalogErrorCode::UnexpectedEof),
            "truncated file error code mismatch");

    io::BufferByteWriter payload_bytes {128};
    io::LittleEndianBinaryWriter payload_writer {payload_bytes};
    for (int index = 0; index < 5; ++index) {
        require(payload_writer.write_u64(1).has_value(), "write malicious id failed");
    }
    require(payload_writer.write_u32(std::numeric_limits<std::uint32_t>::max()).has_value(),
            "write malicious count failed");

    write_catalog_payload(filesystem, path, payload_bytes.bytes());
    auto oversized_count = store.load();
    require(!oversized_count.has_value(), "oversized metadata count should fail");
    require(oversized_count.error().is(catalog::CatalogErrorCode::ResourceLimitExceeded),
            "oversized metadata count error mismatch");
}

void test_remaining_byte_budgets(const std::filesystem::path & path)
{
    auto filesystem = filesystem::create_platform_filesystem();
    catalog::CatalogStore store {path, filesystem};

    const auto expect_resource_limit = [&](auto encode_payload) {
        io::BufferByteWriter payload_bytes {256};
        io::LittleEndianBinaryWriter writer {payload_bytes};
        write_snapshot_id_prefix(writer);
        encode_payload(writer);
        write_catalog_payload(filesystem, path, payload_bytes.bytes());

        auto loaded = store.load();
        require(!loaded.has_value(), "catalog count exceeding remaining bytes should fail");
        require(loaded.error().is(catalog::CatalogErrorCode::ResourceLimitExceeded),
                "remaining-byte budget error code mismatch");
    };

    // 数据库数量在全局上限内，但剩余 payload 不足以容纳一个最小数据库。
    expect_resource_limit(
        [](io::LittleEndianBinaryWriter & writer) {
            require(writer.write_u32(1).has_value(), "write database budget fixture failed");
        }
    );

    // 数据库本身完整，集合数量只违反剩余字节预算。
    expect_resource_limit(
        [](io::LittleEndianBinaryWriter & writer) {
            require(writer.write_u32(1).has_value(), "write collection budget database count failed");
            require(writer.write_u64(1).has_value(), "write collection budget database id failed");
            require(writer.write_string("").has_value(), "write collection budget database name failed");
            require(writer.write_u32(1).has_value(), "write collection budget count failed");
        }
    );

    // 预留后续两个计数字段，使夹具通过集合预算并命中列预算。
    expect_resource_limit(
        [](io::LittleEndianBinaryWriter & writer) {
            require(writer.write_u32(1).has_value(), "write column budget database count failed");
            require(writer.write_u64(1).has_value(), "write column budget database id failed");
            require(writer.write_string("").has_value(), "write column budget database name failed");
            require(writer.write_u32(1).has_value(), "write column budget collection count failed");
            require(writer.write_u64(1).has_value(), "write column budget collection id failed");
            require(writer.write_u64(1).has_value(), "write column budget collection database id failed");
            require(writer.write_string("").has_value(), "write column budget collection name failed");
            require(writer.write_u8(0).has_value(), "write column budget comment marker failed");
            require(writer.write_u32(1).has_value(), "write column budget count failed");
            require(writer.write_u32(0).has_value(), "write column budget index count failed");
            require(writer.write_u32(0).has_value(), "write column budget vector index count failed");
        }
    );

    // 空列集合合法，普通索引数量无法由剩余 payload 支撑。
    expect_resource_limit(
        [](io::LittleEndianBinaryWriter & writer) {
            require(writer.write_u32(1).has_value(), "write index budget database count failed");
            require(writer.write_u64(1).has_value(), "write index budget database id failed");
            require(writer.write_string("").has_value(), "write index budget database name failed");
            require(writer.write_u32(1).has_value(), "write index budget collection count failed");
            require(writer.write_u64(1).has_value(), "write index budget collection id failed");
            require(writer.write_u64(1).has_value(), "write index budget collection database id failed");
            require(writer.write_string("").has_value(), "write index budget collection name failed");
            require(writer.write_u8(0).has_value(), "write index budget comment marker failed");
            require(writer.write_u32(0).has_value(), "write index budget column count failed");
            require(writer.write_u32(1).has_value(), "write index budget count failed");
            require(writer.write_u32(0).has_value(), "write index budget vector index count failed");
        }
    );

    // 空列和普通索引集合合法，向量索引数量违反预算。
    expect_resource_limit(
        [](io::LittleEndianBinaryWriter & writer) {
            require(writer.write_u32(1).has_value(), "write vector budget database count failed");
            require(writer.write_u64(1).has_value(), "write vector budget database id failed");
            require(writer.write_string("").has_value(), "write vector budget database name failed");
            require(writer.write_u32(1).has_value(), "write vector budget collection count failed");
            require(writer.write_u64(1).has_value(), "write vector budget collection id failed");
            require(writer.write_u64(1).has_value(), "write vector budget collection database id failed");
            require(writer.write_string("").has_value(), "write vector budget collection name failed");
            require(writer.write_u8(0).has_value(), "write vector budget comment marker failed");
            require(writer.write_u32(0).has_value(), "write vector budget column count failed");
            require(writer.write_u32(0).has_value(), "write vector budget index count failed");
            require(writer.write_u32(1).has_value(), "write vector budget count failed");
        }
    );

    // element_count 后仅保留列/索引尾部字段，不足一个最小表达式的 10 字节。
    expect_resource_limit(
        [](io::LittleEndianBinaryWriter & writer) {
            require(writer.write_u32(1).has_value(), "write expression budget database count failed");
            require(writer.write_u64(1).has_value(), "write expression budget database id failed");
            require(writer.write_string("").has_value(), "write expression budget database name failed");
            require(writer.write_u32(1).has_value(), "write expression budget collection count failed");
            require(writer.write_u64(1).has_value(), "write expression budget collection id failed");
            require(writer.write_u64(1).has_value(), "write expression budget collection database id failed");
            require(writer.write_string("").has_value(), "write expression budget collection name failed");
            require(writer.write_u8(0).has_value(), "write expression budget collection comment marker failed");
            require(writer.write_u32(1).has_value(), "write expression budget column count failed");
            require(writer.write_u64(1).has_value(), "write expression budget column id failed");
            require(writer.write_string("").has_value(), "write expression budget column name failed");
            require(writer.write_u8(static_cast<std::uint8_t>(common::LogicalTypeId::BigInt)).has_value(),
                    "write expression budget type id failed");
            require(writer.write_u8(0).has_value(), "write expression budget type parameter marker failed");
            require(writer.write_u8(0).has_value(), "write expression budget unique marker failed");
            require(writer.write_u8(1).has_value(), "write expression budget nullable marker failed");
            require(writer.write_u8(1).has_value(), "write expression budget presence marker failed");
            require(writer.write_u8(static_cast<std::uint8_t>(schema::DefaultExpressionKind::Vector)).has_value(),
                    "write expression budget kind failed");
            require(writer.write_u8(static_cast<std::uint8_t>(schema::DefaultLiteralKind::Null)).has_value(),
                    "write expression budget literal kind failed");
            require(writer.write_string("").has_value(), "write expression budget value failed");
            require(writer.write_u32(1).has_value(), "write expression budget element count failed");
            require(writer.write_u8(0).has_value(), "write expression budget column comment marker failed");
            require(writer.write_u32(0).has_value(), "write expression budget index count failed");
            require(writer.write_u32(0).has_value(), "write expression budget vector index count failed");
        }
    );
}

void test_string_limits(const std::filesystem::path & path)
{
    auto filesystem = filesystem::create_platform_filesystem();
    catalog::CatalogStore store {path, filesystem};

    catalog::CatalogSnapshot boundary_snapshot;
    catalog::CatalogDatabaseSnapshot boundary_database;
    boundary_database.name.assign(catalog::MaxStringSize, 'x');
    boundary_snapshot.databases.push_back(std::move(boundary_database));
    require(store.save(boundary_snapshot).has_value(), "save maximum-length catalog string failed");
    auto boundary_loaded = store.load();
    require(boundary_loaded.has_value() && boundary_loaded->has_value(),
            "load maximum-length catalog string failed");
    require((**boundary_loaded).databases[0].name.size() == catalog::MaxStringSize,
            "maximum-length catalog string roundtrip mismatch");

    const std::string oversized_string(catalog::MaxStringSize + 1, 'x');
    const auto require_rejected = [&](const catalog::CatalogSnapshot & snapshot) {
        auto saved = store.save(snapshot);
        require(!saved.has_value(), "oversized catalog string should fail");
        require(saved.error().is(catalog::CatalogErrorCode::ResourceLimitExceeded),
                "oversized catalog string error code mismatch");
    };

    {
        auto snapshot = make_snapshot();
        snapshot.databases[0].name = oversized_string;
        require_rejected(snapshot);
    }
    {
        auto snapshot = make_snapshot();
        snapshot.databases[0].collections[0].name = oversized_string;
        require_rejected(snapshot);
    }
    {
        auto snapshot = make_snapshot();
        snapshot.databases[0].collections[0].comment = oversized_string;
        require_rejected(snapshot);
    }
    {
        auto snapshot = make_snapshot();
        snapshot.databases[0].collections[0].columns[0].name = oversized_string;
        require_rejected(snapshot);
    }
    {
        auto snapshot = make_snapshot();
        snapshot.databases[0].collections[0].columns[0].comment = oversized_string;
        require_rejected(snapshot);
    }
    {
        auto snapshot = make_snapshot();
        snapshot.databases[0].collections[0].columns[1].default_expression->value = oversized_string;
        require_rejected(snapshot);
    }
    {
        auto snapshot = make_snapshot();
        snapshot.databases[0].collections[0].indexes[0].name = oversized_string;
        require_rejected(snapshot);
    }
    {
        auto snapshot = make_snapshot();
        snapshot.databases[0].collections[0].vector_indexes[0].name = oversized_string;
        require_rejected(snapshot);
    }
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
        test_remaining_byte_budgets(path);
        test_string_limits(path);
        std::filesystem::remove_all(directory);
    } catch (const std::exception & exception) {
        std::filesystem::remove_all(directory);
        std::cerr << exception.what() << '\n';
        return 1;
    }
    return 0;
}

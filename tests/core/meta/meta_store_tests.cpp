#include "core/filesystem/platform_filesystem.hpp"
#include "core/filesystem/backend/filesystem_backend.hpp"
#include "core/io/binary_io.hpp"
#include "core/io/file_byte_writer.hpp"
#include "core/meta/meta_store.hpp"

#include <chrono>
#include <exception>
#include <filesystem>
#include <iostream>
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

meta::MetaSnapshot make_snapshot()
{
    meta::MetaSnapshot snapshot;
    snapshot.next_database_id = 2;
    snapshot.next_collection_id = 3;
    snapshot.next_column_id = 5;
    snapshot.next_index_id = 7;
    snapshot.next_vector_index_id = 9;

    meta::MetaSnapshotColumn id;
    id.id = 3;
    id.name = "id";
    id.type = {common::LogicalTypeId::BigInt, std::nullopt};
    id.unique = true;
    id.nullable = false;
    id.comment = "identifier";

    meta::MetaSnapshotColumn embedding;
    embedding.id = 4;
    embedding.name = "embedding";
    embedding.type = {common::LogicalTypeId::Vector, 3};
    embedding.default_expression = meta::entry::DefaultExpression::vector({
        meta::entry::DefaultExpression::literal(meta::entry::DefaultLiteralKind::Float, "0.0"),
        meta::entry::DefaultExpression::literal(meta::entry::DefaultLiteralKind::Float, "0.0"),
        meta::entry::DefaultExpression::literal(meta::entry::DefaultLiteralKind::Float, "0.0"),
    });

    meta::MetaSnapshotCollection collection;
    collection.id = 2;
    collection.database_id = 1;
    collection.name = "items";
    collection.comment = "test collection";
    collection.columns = {id, embedding};
    collection.indexes.push_back({6, {3, 4}, "idx_items", meta::entry::IndexKind::BTree, true});
    collection.vector_indexes.push_back({
        8, 4, "vidx_embedding", meta::entry::VectorIndexKind::Hnsw,
        meta::entry::VectorDistanceMetric::Cosine, 3, 24, 240, 80, 7,
    });

    meta::MetaSnapshotDatabase database;
    database.id = 1;
    database.name = "main";
    database.collections.push_back(std::move(collection));
    snapshot.databases.push_back(std::move(database));
    return snapshot;
}

void test_missing_file_and_roundtrip(const std::filesystem::path & path)
{
    auto filesystem = filesystem::create_platform_filesystem();
    meta::MetaStore store {path, filesystem};
    const auto empty = store.load();
    require(empty.has_value(), "missing meta file should return an empty snapshot");
    require(empty->databases.empty(), "empty snapshot contains a database");

    const auto snapshot = make_snapshot();
    require(store.save(snapshot).has_value(), "save meta snapshot failed");
    const auto loaded = store.load();
    require(loaded.has_value(), "load meta snapshot failed");
    require(loaded->next_vector_index_id == 9, "next id roundtrip mismatch");
    require(loaded->databases.size() == 1, "database count mismatch");
    const auto & collection = loaded->databases[0].collections[0];
    require(collection.database_id == 1, "collection database id mismatch");
    require(collection.columns[1].type.parameter == 3, "logical type parameter mismatch");
    require(collection.columns[1].default_expression->elements.size() == 3, "default expression mismatch");
    require(collection.indexes[0].column_ids.size() == 2, "composite index columns mismatch");
    require(collection.vector_indexes[0].metric == meta::entry::VectorDistanceMetric::Cosine,
            "vector metric mismatch");
}

void write_test_header(
    filesystem::FileSystem & filesystem,
    const std::filesystem::path & path,
    std::uint32_t magic,
    std::optional<std::uint16_t> version
)
{
    auto file = filesystem.open(path, {
        .access = filesystem::backend::FileAccess::ReadWrite,
        .create_mode = filesystem::backend::FileCreateMode::CreateOrTruncate,
    });
    require(file.has_value(), "open test meta file failed");
    io::FileByteWriter byte_writer {*file};
    io::BinaryWriter writer {byte_writer};
    require(writer.write_u32(magic).has_value(), "write test magic failed");
    if (version) {
        require(writer.write_u16(*version).has_value(), "write test version failed");
        require(writer.write_u16(8).has_value(), "write test header size failed");
    }
    require(file->close().has_value(), "close test meta file failed");
}

void test_load_error_codes(const std::filesystem::path & path)
{
    constexpr std::uint32_t meta_magic = 0x544d444c;
    auto filesystem = filesystem::create_platform_filesystem();
    meta::MetaStore store {path, filesystem};

    write_test_header(filesystem, path, 0, 1);
    auto invalid_magic = store.load();
    require(!invalid_magic.has_value(), "invalid magic should fail");
    require(invalid_magic.error().code == meta::MetaStoreErrorCode::InvalidFormat,
            "invalid magic error code mismatch");

    write_test_header(filesystem, path, meta_magic, 2);
    auto unsupported_version = store.load();
    require(!unsupported_version.has_value(), "unsupported version should fail");
    require(unsupported_version.error().code == meta::MetaStoreErrorCode::UnsupportedVersion,
            "unsupported version error code mismatch");

    write_test_header(filesystem, path, meta_magic, std::nullopt);
    auto truncated = store.load();
    require(!truncated.has_value(), "truncated meta file should fail");
    require(truncated.error().code == meta::MetaStoreErrorCode::UnexpectedEof,
            "truncated file error code mismatch");
}

} // namespace

int main()
{
    const auto suffix = std::chrono::steady_clock::now().time_since_epoch().count();
    const auto directory = std::filesystem::temp_directory_path() / ("litedb_meta_store_" + std::to_string(suffix));
    const auto path = directory / "meta.ldb";
    try {
        test_missing_file_and_roundtrip(path);
        test_load_error_codes(path);
        std::filesystem::remove_all(directory);
    } catch (const std::exception & exception) {
        std::filesystem::remove_all(directory);
        std::cerr << exception.what() << '\n';
        return 1;
    }
    return 0;
}

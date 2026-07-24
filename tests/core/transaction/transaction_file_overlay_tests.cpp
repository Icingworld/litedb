#include <array>
#include <chrono>
#include <filesystem>
#include <fstream>
#include <stdexcept>
#include <vector>

#include "core/filesystem/platform_filesystem.hpp"
#include "core/transaction/transaction_file_overlay.hpp"

namespace
{
using namespace litedb::core;

void require(bool condition, const char * message)
{
    if (!condition) throw std::runtime_error(message);
}

std::filesystem::path temporary_directory()
{
    return std::filesystem::temp_directory_path() /
        ("litedb-overlay-" +
         std::to_string(std::chrono::steady_clock::now().time_since_epoch().count()));
}

void write_bytes(const std::filesystem::path & path, const std::vector<std::byte> & bytes)
{
    std::filesystem::create_directories(path.parent_path());
    std::ofstream output(path, std::ios::binary | std::ios::trunc);
    output.write(reinterpret_cast<const char *>(bytes.data()), static_cast<std::streamsize>(bytes.size()));
    require(output.good(), "failed to initialize overlay fixture");
}

std::vector<std::byte> read_bytes(const std::filesystem::path & path)
{
    std::ifstream input(path, std::ios::binary | std::ios::ate);
    require(input.good(), "failed to open applied overlay file");
    const auto size = input.tellg();
    std::vector<std::byte> bytes(static_cast<std::size_t>(size));
    input.seekg(0);
    input.read(reinterpret_cast<char *>(bytes.data()), static_cast<std::streamsize>(bytes.size()));
    require(input.good() || input.eof(), "failed to read applied overlay file");
    return bytes;
}

void test_sparse_read_write_extend_and_truncate()
{
    const auto base = temporary_directory();
    const auto logical = base / ".transactions" / "txn_1";
    const auto live_path = base / "collections" / "7.store";
    const auto staged_path = logical / "collections" / "7.store";
    std::vector<std::byte> initial(3 * 4096, std::byte {0x11});
    write_bytes(live_path, initial);

    auto filesystem = filesystem::create_platform_filesystem();
    transaction::TransactionFileOverlay overlay {logical, base, filesystem};
    auto file = overlay.filesystem().open(staged_path, {
        filesystem::FileAccess::ReadWrite,
        filesystem::FileCreateMode::OpenExisting,
    });
    require(file.has_value(), "overlay open-existing failed");

    std::array<std::byte, 20> cross_block {};
    cross_block.fill(std::byte {0x44});
    require(file->write_at(4090, cross_block).has_value(), "cross-block overlay write failed");
    const std::array extension {std::byte {0x55}, std::byte {0x66}};
    require(file->write_at(initial.size() + 9, extension).has_value(), "overlay extension failed");
    require(file->size().value_or(0) == initial.size() + 11, "overlay logical length mismatch");
    require(file->truncate(9000).has_value(), "overlay truncate failed");

    std::array<std::byte, 20> observed {};
    require(file->read_at(4090, observed).value_or(0) == observed.size() &&
            observed == cross_block, "overlay read-after-write mismatch");

    auto batch = overlay.export_batch();
    require(batch.has_value(), "overlay export failed");
    std::size_t overwrite_count {0};
    std::size_t truncate_count {0};
    for (const auto & write : batch->writes()) {
        overwrite_count += write.mode == wal::FileWriteMode::Overwrite;
        truncate_count += write.mode == wal::FileWriteMode::Truncate;
    }
    require(overwrite_count == 1, "adjacent dirty blocks were not coalesced");
    require(truncate_count == 1, "final logical length was not exported");
    require(batch->apply(base, filesystem, false).has_value(), "overlay batch apply failed");

    auto applied = read_bytes(live_path);
    require(applied.size() == 9000, "applied truncate length mismatch");
    require(std::equal(cross_block.begin(), cross_block.end(), applied.begin() + 4090),
            "applied cross-block bytes mismatch");
    std::filesystem::remove_all(base);
}

void test_unchanged_create_delete_and_atomic_replace()
{
    const auto base = temporary_directory();
    const auto logical = base / ".transactions" / "txn_2";
    const auto collection = base / "collections" / "8.store";
    write_bytes(collection, std::vector<std::byte>(4096, std::byte {0x22}));
    write_bytes(base / "meta.lmeta", {std::byte {0x01}});

    auto filesystem = filesystem::create_platform_filesystem();
    transaction::TransactionFileOverlay overlay {logical, base, filesystem};
    auto unchanged = overlay.filesystem().open(logical / "collections" / "8.store", {
        filesystem::FileAccess::ReadWrite,
        filesystem::FileCreateMode::OpenExisting,
    });
    require(unchanged.has_value(), "unchanged overlay file open failed");
    const std::array same {std::byte {0x22}, std::byte {0x22}};
    require(unchanged->write_at(100, same).has_value(), "unchanged overlay write failed");

    require(overlay.filesystem().remove(logical / "collections" / "8.store").has_value(),
            "overlay delete failed");
    auto temporary = overlay.filesystem().open(logical / "meta.lmeta.tmp", {
        filesystem::FileAccess::ReadWrite,
        filesystem::FileCreateMode::CreateOrTruncate,
    });
    require(temporary.has_value(), "overlay temporary create failed");
    const std::array replacement {std::byte {0x09}, std::byte {0x08}, std::byte {0x07}};
    require(temporary->write_at(0, replacement).has_value(), "overlay temporary write failed");
    require(temporary->close().has_value(), "overlay temporary close failed");
    require(overlay.filesystem().replace_file_atomic(
        logical / "meta.lmeta.tmp",
        logical / "meta.lmeta"
    ).has_value(), "overlay atomic replace failed");

    auto batch = overlay.export_batch();
    require(batch.has_value(), "lifecycle overlay export failed");
    bool deleted {false};
    bool replaced_meta {false};
    for (const auto & write : batch->writes()) {
        deleted = deleted || (write.target.kind == wal::FileKind::CollectionStore &&
                              write.mode == wal::FileWriteMode::Delete);
        replaced_meta = replaced_meta || (write.target.kind == wal::FileKind::MetaStore &&
                                          write.mode == wal::FileWriteMode::Overwrite);
    }
    require(deleted && replaced_meta, "overlay lifecycle changes were not exported");
    require(batch->apply(base, filesystem, false).has_value(), "lifecycle overlay apply failed");
    require(!std::filesystem::exists(collection), "overlay delete was not applied");
    require(read_bytes(base / "meta.lmeta") ==
            std::vector<std::byte>(replacement.begin(), replacement.end()),
            "overlay atomic replacement bytes mismatch");
    std::filesystem::remove_all(base);
}

} // namespace

int main()
{
    test_sparse_read_write_extend_and_truncate();
    test_unchanged_create_delete_and_atomic_replace();
    return 0;
}

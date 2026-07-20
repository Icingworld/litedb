#include "core/filesystem/backend/filesystem_backend.hpp"
#include "core/filesystem/platform_filesystem.hpp"
#include "core/wal/file_write_batch.hpp"
#include "core/wal/wal_store.hpp"

#include <array>
#include <filesystem>
#include <stdexcept>

namespace
{
using namespace litedb::core;

void require(bool condition, const char * message)
{
    if (!condition) throw std::runtime_error(message);
}

std::filesystem::path temp_dir()
{
    auto path = std::filesystem::temp_directory_path() / "litedb_wal_tests";
    std::filesystem::remove_all(path);
    std::filesystem::create_directories(path);
    return path;
}
}

int main()
{
    const auto directory = temp_dir();
    auto filesystem = filesystem::create_platform_filesystem();
    auto store = wal::WalStore::open(directory / "wal" / "litedb.wal", filesystem);
    require(store.has_value(), "open WAL failed");

    const wal::FileWrite write {
        .target = {.kind = wal::FileKind::CollectionStore, .object_id = 7},
        .offset = 11,
        .after_image = {std::byte {1}, std::byte {2}, std::byte {3}},
    };
    auto begin = store->append_begin(1);
    auto mutation = store->append_write(1, write);
    auto commit = store->append_commit(1);
    require(begin && mutation && commit, "append WAL transaction failed");
    require(store->flush_through(*commit).has_value(), "flush WAL failed");

    auto scanned = store->scan();
    require(scanned && scanned->records.size() == 3, "scan WAL failed");
    require(scanned->maximum_transaction_id == 1, "maximum transaction id mismatch");
    auto decoded = wal::WalCodec::decode_file_write(scanned->records[1].payload);
    require(decoded && decoded->target == write.target && decoded->offset == write.offset,
            "decode file write failed");
    require(decoded->after_image == write.after_image, "file write bytes mismatch");

    wal::FileWriteBatch batch;
    batch.add(write);
    const std::array base {std::byte {0}, std::byte {0}, std::byte {0}, std::byte {0}};
    auto overlaid = batch.read(write.target, 10, base);
    require(overlaid && (*overlaid)[0] == std::byte {0} && (*overlaid)[1] == std::byte {1} &&
            (*overlaid)[3] == std::byte {3}, "overlay read mismatch");

    const auto committed_size = std::filesystem::file_size(store->path());
    {
        auto file = filesystem.open(store->path(), filesystem::backend::FileOpenOptions {
            .access = filesystem::backend::FileAccess::ReadWrite,
            .create_mode = filesystem::backend::FileCreateMode::OpenExisting,
        });
        require(file.has_value(), "open WAL for truncation failed");
        const std::array garbage {std::byte {4}, std::byte {5}};
        require(file->append(garbage).has_value(), "append incomplete tail failed");
    }
    auto truncated = store->scan();
    require(truncated && truncated->truncated_tail, "incomplete tail not detected");
    require(std::filesystem::file_size(store->path()) == committed_size, "incomplete tail not truncated");

    {
        auto file = filesystem.open(store->path(), filesystem::backend::FileOpenOptions {
            .access = filesystem::backend::FileAccess::ReadWrite,
            .create_mode = filesystem::backend::FileCreateMode::OpenExisting,
        });
        require(file.has_value(), "open WAL for checksum corruption failed");
        std::array<std::byte, 1> checksum_byte {};
        require(file->read_at(*mutation + 40, checksum_byte).value_or(0) == 1, "read checksum byte failed");
        checksum_byte[0] ^= std::byte {1};
        require(file->write_at(*mutation + 40, checksum_byte).has_value(), "corrupt checksum failed");
    }
    auto corrupted = store->scan(false);
    require(!corrupted && corrupted.error().code == wal::WalErrorCode::CorruptedRecord,
            "complete checksum corruption should be rejected");
    return 0;
}

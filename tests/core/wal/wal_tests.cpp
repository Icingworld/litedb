#include "core/filesystem/backend/filesystem_backend.hpp"
#include "core/filesystem/platform_filesystem.hpp"
#include "core/wal/file_write_batch.hpp"
#include "core/wal/wal_manager.hpp"
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
    auto store = wal::WalStore::open(directory / "wal" / "litedb.wal", filesystem, wal::WalFileHeader {});
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
    require(decoded->mode == wal::FileWriteMode::Overwrite, "legacy overwrite mode mismatch");

    const wal::FileWrite replacement {
        .target = {.kind = wal::FileKind::MetaStore, .object_id = 0},
        .offset = 0,
        .after_image = {std::byte {7}, std::byte {8}},
        .mode = wal::FileWriteMode::Replace,
    };
    auto replacement_decoded = wal::WalCodec::decode_file_write(wal::WalCodec::encode_file_write(replacement));
    require(replacement_decoded && replacement_decoded->mode == wal::FileWriteMode::Replace &&
            replacement_decoded->target == replacement.target && replacement_decoded->after_image == replacement.after_image,
            "replace operation codec mismatch");

    wal::FileWriteBatch lifecycle;
    lifecycle.add(replacement);
    require(lifecycle.apply(directory, filesystem, true).has_value(), "replace operation apply failed");
    require(std::filesystem::file_size(directory / "meta.lmeta") == 2, "replace operation size mismatch");
    wal::FileWriteBatch deletion;
    deletion.add(wal::FileWrite {
        .target = replacement.target,
        .offset = 0,
        .after_image = {},
        .mode = wal::FileWriteMode::Delete,
    });
    require(deletion.apply(directory, filesystem, true).has_value(), "delete operation apply failed");
    require(!std::filesystem::exists(directory / "meta.lmeta"), "delete operation did not remove target");

    wal::FileWriteBatch batch;
    batch.add(write);
    const std::array base {std::byte {0}, std::byte {0}, std::byte {0}, std::byte {0}};
    auto overlaid = batch.read(write.target, 10, base);
    require(overlaid && (*overlaid)[0] == std::byte {0} && (*overlaid)[1] == std::byte {1} &&
            (*overlaid)[3] == std::byte {3}, "overlay read mismatch");

    const auto committed_size = std::filesystem::file_size(store->path());
    {
        auto file = filesystem.open(store->path(), filesystem::FileOpenOptions {
            .access = filesystem::FileAccess::ReadWrite,
            .create_mode = filesystem::FileCreateMode::OpenExisting,
        });
        require(file.has_value(), "open WAL for truncation failed");
        const std::array garbage {std::byte {4}, std::byte {5}};
        require(file->append(garbage).has_value(), "append incomplete tail failed");
    }
    auto truncated = store->scan();
    require(truncated && truncated->truncated_tail, "incomplete tail not detected");
    require(std::filesystem::file_size(store->path()) == committed_size, "incomplete tail not truncated");

    {
        auto file = filesystem.open(store->path(), filesystem::FileOpenOptions {
            .access = filesystem::FileAccess::ReadWrite,
            .create_mode = filesystem::FileCreateMode::OpenExisting,
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

    const auto segmented_directory = directory / "segmented";
    {
        auto manager = wal::WalManager::open(segmented_directory, filesystem);
        require(manager.has_value(), "open segmented WAL failed");
        auto manager_begin = manager->append_begin(11);
        auto manager_commit = manager->append_commit(11);
        require(manager_begin && manager_commit && manager->flush_through(*manager_commit),
                "append segmented WAL transaction failed");
        require(manager->rotate(11).has_value(), "rotate segmented WAL failed");
        const auto metrics = manager->metrics();
        require(metrics.generation == 2 && metrics.checkpoint_transaction_id == 11 &&
                    metrics.size_bytes == wal::WalCodec::FileHeaderSize,
                "segmented WAL rotation metadata mismatch");
    }

    const auto temporary = segmented_directory / "00000000000000000003.wal.tmp";
    {
        auto file = filesystem.open(temporary, filesystem::FileOpenOptions {
            .access = filesystem::FileAccess::ReadWrite,
            .create_mode = filesystem::FileCreateMode::CreateOrTruncate,
        });
        require(file.has_value(), "create stale WAL temporary file failed");
        const std::array garbage {std::byte {1}};
        require(file->write_at(0, garbage).has_value(), "write stale WAL temporary file failed");
    }
    {
        auto manager = wal::WalManager::open(segmented_directory, filesystem);
        require(manager.has_value() && !std::filesystem::exists(temporary),
                "startup did not ignore and clean stale WAL temporary file");
    }

    const auto lower_path = segmented_directory / "00000000000000000001.wal";
    {
        auto lower = wal::WalStore::open(lower_path, filesystem, wal::WalFileHeader {.generation = 1});
        require(lower.has_value(), "create lower WAL generation failed");
    }
    const auto highest_path = segmented_directory / "00000000000000000002.wal";
    {
        auto file = filesystem.open(highest_path, filesystem::FileOpenOptions {
            .access = filesystem::FileAccess::ReadWrite,
            .create_mode = filesystem::FileCreateMode::OpenExisting,
        });
        require(file.has_value(), "open highest WAL generation failed");
        std::array<std::byte, 1> checksum_byte {};
        require(file->read_at(24, checksum_byte).value_or(0) == 1, "read WAL header checksum failed");
        checksum_byte[0] ^= std::byte {1};
        require(file->write_at(24, checksum_byte).has_value(), "corrupt WAL header failed");
    }
    auto refused_fallback = wal::WalManager::open(segmented_directory, filesystem);
    require(!refused_fallback && refused_fallback.error().code == wal::WalErrorCode::InvalidFormat,
            "WAL manager fell back from a corrupted highest generation");
    return 0;
}

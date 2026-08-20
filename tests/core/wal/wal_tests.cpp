#include "core/filesystem/platform_filesystem.hpp"
#include "core/wal/file_write_batch.hpp"
#include "core/wal/wal_codec.hpp"
#include "core/wal/wal_manager.hpp"
#include "core/wal/wal_store.hpp"

#include <algorithm>
#include <array>
#include <filesystem>
#include <stdexcept>
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

std::filesystem::path temp_dir()
{
    auto path = std::filesystem::temp_directory_path() / "litedb_wal_tests";
    std::filesystem::remove_all(path);
    std::filesystem::create_directories(path);
    return path;
}

std::vector<std::byte> read_bytes(
    filesystem::FileSystem & filesystem,
    const std::filesystem::path & path,
    std::uint64_t offset,
    std::size_t size
)
{
    auto file = filesystem.open(
        path,
        {
            .access = filesystem::FileAccess::ReadOnly,
            .create_mode = filesystem::FileCreateMode::OpenExisting,
        }
    );
    require(file.has_value(), "open fixture file failed");
    std::vector<std::byte> bytes(size);
    auto read = file->read_at(offset, bytes);
    require(read.has_value() && *read == size, "read fixture bytes failed");
    return bytes;
}

} // namespace

int main()
{
    const auto directory = temp_dir();
    auto filesystem = filesystem::create_platform_filesystem();
    const auto path = directory / "wal" / "00000000000000000001.wal";

    auto encoded_header = wal::WalCodec::encode_file_header(wal::WalFileHeader {});
    require(encoded_header.has_value(), "encode WAL header failed");
    const std::array<std::byte, wal::WalCodec::FileHeaderSize> header_fixture {
        std::byte {76},
        std::byte {68},
        std::byte {87},
        std::byte {76},
        std::byte {2},
        std::byte {0},
        std::byte {32},
        std::byte {0},
        std::byte {1},
        std::byte {0},
        std::byte {0},
        std::byte {0},
        std::byte {0},
        std::byte {0},
        std::byte {0},
        std::byte {0},
        std::byte {0},
        std::byte {0},
        std::byte {0},
        std::byte {0},
        std::byte {0},
        std::byte {0},
        std::byte {0},
        std::byte {0},
        std::byte {138},
        std::byte {142},
        std::byte {225},
        std::byte {149},
        std::byte {0},
        std::byte {0},
        std::byte {0},
        std::byte {0},
    };
    require(*encoded_header == header_fixture, "WAL header golden fixture changed");

    auto store = wal::WalStore::create(path, filesystem, wal::WalFileHeader {});
    require(store.has_value(), "create WAL failed");
    require(
        read_bytes(filesystem, path, 0, header_fixture.size()) ==
            std::vector<std::byte>(header_fixture.begin(), header_fixture.end()),
        "persisted WAL header fixture changed"
    );

    auto duplicate = wal::WalStore::create(path, filesystem, wal::WalFileHeader {});
    require(
        !duplicate && duplicate.error().is(filesystem::FileSystemErrorCode::AlreadyExists),
        "duplicate WAL create did not preserve AlreadyExists"
    );

    const auto missing_path = directory / "missing" / "absent.wal";
    auto missing = wal::WalStore::open(missing_path, filesystem);
    require(
        !missing && missing.error().is(filesystem::FileSystemErrorCode::NotFound),
        "missing WAL did not preserve NotFound"
    );
    require(
        !std::filesystem::exists(missing_path.parent_path()),
        "open created a missing parent directory"
    );

    const auto empty_path = directory / "empty.wal";
    {
        auto file = filesystem.open(
            empty_path,
            {
                .access = filesystem::FileAccess::ReadWrite,
                .create_mode = filesystem::FileCreateMode::CreateNew,
            }
        );
        require(file.has_value(), "create empty WAL fixture failed");
    }
    auto empty = wal::WalStore::open(empty_path, filesystem);
    require(
        !empty && empty.error().is(wal::WalErrorCode::InvalidFormat),
        "empty WAL header was accepted"
    );

    const auto truncated_path = directory / "truncated.wal";
    {
        auto file = filesystem.open(
            truncated_path,
            {
                .access = filesystem::FileAccess::ReadWrite,
                .create_mode = filesystem::FileCreateMode::CreateNew,
            }
        );
        require(file.has_value(), "create truncated WAL fixture failed");
        const std::array<std::byte, 1> byte {std::byte {1}};
        require(file->write_at(0, byte).has_value(), "write truncated WAL fixture failed");
    }
    auto truncated_header = wal::WalStore::open(truncated_path, filesystem);
    require(
        !truncated_header && truncated_header.error().is(wal::WalErrorCode::InvalidFormat),
        "truncated WAL header was accepted"
    );

    const wal::FileWrite write {
        .target = {.kind = wal::FileKind::CollectionStore, .object_id = 7},
        .offset = 11,
        .after_image = {std::byte {1}, std::byte {2}, std::byte {3}},
    };
    auto payload = wal::WalCodec::encode_file_write(write);
    require(payload.has_value(), "encode file-write payload failed");
    auto golden_record =
        wal::WalCodec::encode_record(wal::WalRecordType::FileWrite, 32, 1, *payload);
    require(golden_record.has_value(), "encode WAL record failed");
    const std::array<std::byte, 4> record_magic {
        std::byte {76},
        std::byte {87},
        std::byte {82},
        std::byte {49},
    };
    require(
        std::equal(record_magic.begin(), record_magic.end(), golden_record->begin()),
        "WAL record magic fixture changed"
    );
    require(
        golden_record->size() == wal::WalCodec::RecordHeaderSize + payload->size(),
        "WAL record size fixture changed"
    );

    const std::array<std::byte, 48> begin_fixture {
        std::byte {76},
        std::byte {87},
        std::byte {82},
        std::byte {49},
        std::byte {2},
        std::byte {0},
        std::byte {1},
        std::byte {0},
        std::byte {48},
        std::byte {0},
        std::byte {0},
        std::byte {0},
        std::byte {0},
        std::byte {0},
        std::byte {0},
        std::byte {0},
        std::byte {32},
        std::byte {0},
        std::byte {0},
        std::byte {0},
        std::byte {0},
        std::byte {0},
        std::byte {0},
        std::byte {0},
        std::byte {1},
        std::byte {0},
        std::byte {0},
        std::byte {0},
        std::byte {0},
        std::byte {0},
        std::byte {0},
        std::byte {0},
        std::byte {0},
        std::byte {0},
        std::byte {0},
        std::byte {0},
        std::byte {0},
        std::byte {0},
        std::byte {0},
        std::byte {0},
        std::byte {53},
        std::byte {198},
        std::byte {208},
        std::byte {25},
        std::byte {0},
        std::byte {0},
        std::byte {0},
        std::byte {0},
    };
    const std::array<std::byte, 75> write_fixture {
        std::byte {76},
        std::byte {87},
        std::byte {82},
        std::byte {49},
        std::byte {2},
        std::byte {0},
        std::byte {2},
        std::byte {0},
        std::byte {75},
        std::byte {0},
        std::byte {0},
        std::byte {0},
        std::byte {0},
        std::byte {0},
        std::byte {0},
        std::byte {0},
        std::byte {80},
        std::byte {0},
        std::byte {0},
        std::byte {0},
        std::byte {0},
        std::byte {0},
        std::byte {0},
        std::byte {0},
        std::byte {1},
        std::byte {0},
        std::byte {0},
        std::byte {0},
        std::byte {0},
        std::byte {0},
        std::byte {0},
        std::byte {0},
        std::byte {27},
        std::byte {0},
        std::byte {0},
        std::byte {0},
        std::byte {0},
        std::byte {0},
        std::byte {0},
        std::byte {0},
        std::byte {92},
        std::byte {220},
        std::byte {83},
        std::byte {136},
        std::byte {0},
        std::byte {0},
        std::byte {0},
        std::byte {0},
        std::byte {1},
        std::byte {0},
        std::byte {0},
        std::byte {0},
        std::byte {0},
        std::byte {0},
        std::byte {0},
        std::byte {0},
        std::byte {7},
        std::byte {0},
        std::byte {0},
        std::byte {0},
        std::byte {0},
        std::byte {0},
        std::byte {0},
        std::byte {0},
        std::byte {11},
        std::byte {0},
        std::byte {0},
        std::byte {0},
        std::byte {0},
        std::byte {0},
        std::byte {0},
        std::byte {0},
        std::byte {1},
        std::byte {2},
        std::byte {3},
    };
    const std::array<std::byte, 48> commit_fixture {
        std::byte {76},
        std::byte {87},
        std::byte {82},
        std::byte {49},
        std::byte {2},
        std::byte {0},
        std::byte {3},
        std::byte {0},
        std::byte {48},
        std::byte {0},
        std::byte {0},
        std::byte {0},
        std::byte {0},
        std::byte {0},
        std::byte {0},
        std::byte {0},
        std::byte {155},
        std::byte {0},
        std::byte {0},
        std::byte {0},
        std::byte {0},
        std::byte {0},
        std::byte {0},
        std::byte {0},
        std::byte {1},
        std::byte {0},
        std::byte {0},
        std::byte {0},
        std::byte {0},
        std::byte {0},
        std::byte {0},
        std::byte {0},
        std::byte {0},
        std::byte {0},
        std::byte {0},
        std::byte {0},
        std::byte {0},
        std::byte {0},
        std::byte {0},
        std::byte {0},
        std::byte {48},
        std::byte {49},
        std::byte {249},
        std::byte {103},
        std::byte {0},
        std::byte {0},
        std::byte {0},
        std::byte {0},
    };
    auto encoded_begin = wal::WalCodec::encode_record(wal::WalRecordType::Begin, 32, 1, {});
    auto encoded_write =
        wal::WalCodec::encode_record(wal::WalRecordType::FileWrite, 80, 1, *payload);
    auto encoded_commit = wal::WalCodec::encode_record(wal::WalRecordType::Commit, 155, 1, {});
    require(
        encoded_begin &&
            *encoded_begin == std::vector<std::byte>(begin_fixture.begin(), begin_fixture.end()),
        "Begin record golden fixture changed"
    );
    require(
        encoded_write &&
            *encoded_write == std::vector<std::byte>(write_fixture.begin(), write_fixture.end()),
        "FileWrite record golden fixture changed"
    );
    require(
        encoded_commit &&
            *encoded_commit == std::vector<std::byte>(commit_fixture.begin(), commit_fixture.end()),
        "Commit record golden fixture changed"
    );

    auto begin = store->append_begin(1);
    auto mutation = store->append_write(1, write);
    auto commit = store->append_commit(1);
    require(begin && mutation && commit, "append WAL transaction failed");
    require(store->flush_through(*commit).has_value(), "flush WAL failed");
    require(
        read_bytes(filesystem, path, 32, begin_fixture.size()) ==
            std::vector<std::byte>(begin_fixture.begin(), begin_fixture.end()),
        "persisted Begin record fixture changed"
    );
    require(
        read_bytes(filesystem, path, 80, write_fixture.size()) ==
            std::vector<std::byte>(write_fixture.begin(), write_fixture.end()),
        "persisted FileWrite record fixture changed"
    );
    require(
        read_bytes(filesystem, path, 155, commit_fixture.size()) ==
            std::vector<std::byte>(commit_fixture.begin(), commit_fixture.end()),
        "persisted Commit record fixture changed"
    );
    auto reopened = wal::WalStore::open(path, filesystem);
    require(reopened && reopened->header().generation == 1, "reopen existing v2 WAL failed");

    auto scanned = store->scan();
    require(scanned && scanned->records.size() == 3, "scan WAL failed");
    require(scanned->maximum_transaction_id == 1, "maximum transaction id mismatch");
    auto decoded = wal::WalCodec::decode_file_write(scanned->records[1].payload);
    require(
        decoded && decoded->target == write.target && decoded->offset == write.offset,
        "decode file write failed"
    );
    require(decoded->after_image == write.after_image, "file write bytes mismatch");

    wal::WalDecodeLimits strict_limits {
        .max_record_size_bytes = wal::WalCodec::RecordHeaderSize,
        .max_scan_size_bytes = 1024 * 1024,
        .max_record_count = 16,
    };
    auto limited = store->scan(false, strict_limits);
    require(
        !limited && limited.error().is(wal::WalErrorCode::ResourceLimitExceeded),
        "oversized WAL record should be rejected by the decode budget"
    );
    const auto * limit_context = limited.error().context<wal::WalErrorContext>();
    require(
        limit_context != nullptr && limit_context->operation == wal::WalOperation::Scan &&
            limit_context->path == store->path() && limit_context->lsn.has_value(),
        "WAL resource-limit error context mismatch"
    );

    wal::FileWriteBatch merged;
    merged.add(
        wal::FileWrite {
            .target = write.target,
            .offset = 0,
            .after_image = {std::byte {1}, std::byte {2}},
        }
    );
    merged.add(
        wal::FileWrite {
            .target = write.target,
            .offset = 2,
            .after_image = {std::byte {3}},
        }
    );
    require(
        merged.normalize().has_value() && merged.writes().size() == 1 &&
            merged.writes()[0].after_image.size() == 3,
        "FileWriteBatch normalization did not merge adjacent writes"
    );
    wal::FileWriteBatch overlap;
    overlap.add(
        wal::FileWrite {
            .target = write.target,
            .offset = 0,
            .after_image = {std::byte {1}, std::byte {2}}
        }
    );
    overlap.add(
        wal::FileWrite {.target = write.target, .offset = 1, .after_image = {std::byte {3}}}
    );
    require(!overlap.normalize(), "FileWriteBatch accepted overlapping writes");
    auto invalid_target = wal::FileWriteBatch::resolve_target(
        directory,
        wal::FileTarget {.kind = static_cast<wal::FileKind>(99), .object_id = 1}
    );
    require(
        !invalid_target && invalid_target.error().is(wal::WalErrorCode::InvalidRecord),
        "invalid FileKind fell back to an empty path"
    );

    const wal::FileWrite replacement {
        .target = {.kind = wal::FileKind::CatalogStore, .object_id = 0},
        .offset = 0,
        .after_image = {std::byte {7}, std::byte {8}},
        .mode = wal::FileWriteMode::Replace,
    };
    auto replacement_payload = wal::WalCodec::encode_file_write(replacement);
    require(replacement_payload.has_value(), "encode replacement payload failed");
    auto replacement_decoded = wal::WalCodec::decode_file_write(std::move(*replacement_payload));
    require(
        replacement_decoded && replacement_decoded->mode == wal::FileWriteMode::Replace &&
            replacement_decoded->target == replacement.target &&
            replacement_decoded->after_image == replacement.after_image,
        "replace operation codec mismatch"
    );
    wal::FileWriteBatch lifecycle;
    lifecycle.add(replacement);
    require(
        lifecycle.apply(directory, filesystem, true).has_value(),
        "replace operation apply failed"
    );
    require(
        std::filesystem::file_size(directory / "catalog.lcat") == 2,
        "replace operation size mismatch"
    );
    wal::FileWriteBatch deletion;
    deletion.add(
        wal::FileWrite {
            .target = replacement.target,
            .offset = 0,
            .after_image = {},
            .mode = wal::FileWriteMode::Delete,
        }
    );
    require(
        deletion.apply(directory, filesystem, true).has_value(),
        "delete operation apply failed"
    );
    require(
        !std::filesystem::exists(directory / "catalog.lcat"),
        "delete operation did not remove target"
    );

    const auto committed_size = std::filesystem::file_size(store->path());
    {
        auto file = filesystem.open(
            store->path(),
            {
                .access = filesystem::FileAccess::ReadWrite,
                .create_mode = filesystem::FileCreateMode::OpenExisting,
            }
        );
        require(file.has_value(), "open WAL for truncation failed");
        const std::array<std::byte, 1> garbage {std::byte {4}};
        require(file->append(garbage).has_value(), "append incomplete tail failed");
    }
    auto truncated = store->scan();
    require(truncated && truncated->truncated_tail, "incomplete tail not detected");
    require(
        std::filesystem::file_size(store->path()) == committed_size,
        "incomplete tail not truncated"
    );

    const auto counted_directory = directory / "counted";
    {
        auto manager = wal::WalManager::open(counted_directory, filesystem);
        require(manager.has_value(), "open counted WAL failed");
        auto counted_begin = manager->append_begin(21);
        auto counted_commit = manager->append_commit(21);
        require(
            counted_begin && counted_commit && manager->flush_through(*counted_commit),
            "append counted WAL transaction failed"
        );
    }
    auto counted = wal::WalManager::open(
        counted_directory,
        filesystem,
        {
            .max_record_size_bytes = 1024,
            .max_scan_size_bytes = 1024 * 1024,
            .max_record_count = 3,
        }
    );
    require(counted.has_value(), "reopen counted WAL failed");
    auto counted_budget = counted->validate_transaction({});
    require(
        !counted_budget && counted_budget.error().is(wal::WalErrorCode::ResourceLimitExceeded),
        "reopened WAL did not initialize its record count"
    );

    const auto segmented_directory = directory / "segmented";
    {
        auto manager = wal::WalManager::open(segmented_directory, filesystem);
        require(manager.has_value(), "open segmented WAL failed");
        auto manager_begin = manager->append_begin(11);
        auto manager_commit = manager->append_commit(11);
        require(
            manager_begin && manager_commit && manager->flush_through(*manager_commit),
            "append segmented WAL transaction failed"
        );
        require(manager->rotate(11).has_value(), "rotate segmented WAL failed");
        const auto metrics = manager->metrics();
        require(
            metrics.generation == 2 && metrics.checkpoint_transaction_id == 11 &&
                metrics.size_bytes == wal::WalCodec::FileHeaderSize,
            "segmented WAL rotation metadata mismatch"
        );
    }

    const auto highest_path = segmented_directory / "00000000000000000002.wal";
    {
        auto file = filesystem.open(
            highest_path,
            {
                .access = filesystem::FileAccess::ReadWrite,
                .create_mode = filesystem::FileCreateMode::OpenExisting,
            }
        );
        require(file.has_value(), "open highest WAL generation failed");
        std::array<std::byte, 1> checksum_byte {};
        require(
            file->read_at(24, checksum_byte).value_or(0) == 1,
            "read WAL header checksum failed"
        );
        checksum_byte[0] ^= std::byte {1};
        require(file->write_at(24, checksum_byte).has_value(), "corrupt WAL header failed");
    }
    auto refused_fallback = wal::WalManager::open(segmented_directory, filesystem);
    require(
        !refused_fallback && refused_fallback.error().is(wal::WalErrorCode::InvalidFormat),
        "WAL manager fell back from a corrupted highest generation"
    );
    return 0;
}

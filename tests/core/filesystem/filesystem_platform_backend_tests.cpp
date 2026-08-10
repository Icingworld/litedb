#include "core/filesystem/platform_filesystem.hpp"

#include <algorithm>
#include <array>
#include <barrier>
#include <cstddef>
#include <cstdint>
#include <filesystem>
#include <limits>
#include <random>
#include <stdexcept>
#include <string>
#include <thread>
#include <vector>

namespace
{

using namespace litedb::core::filesystem;
using namespace litedb::core::filesystem::backend;

void require(bool condition, const char * message)
{
    if (!condition) {
        throw std::runtime_error(message);
    }
}

std::filesystem::path make_temp_dir()
{
    const auto suffix = std::to_string(std::random_device {}());
    auto path = std::filesystem::temp_directory_path() /
                ("litedb_filesystem_platform_backend_tests_" + suffix);
    std::filesystem::remove_all(path);
    std::filesystem::create_directories(path);
    return path;
}

std::vector<std::byte> bytes(std::initializer_list<unsigned char> values)
{
    std::vector<std::byte> result;
    result.reserve(values.size());
    for (const auto value : values) {
        result.push_back(static_cast<std::byte>(value));
    }
    return result;
}

bool contains_filename(const std::vector<std::filesystem::path> & entries, const char * name)
{
    return std::ranges::any_of(entries, [name](const std::filesystem::path & entry) {
        return entry.filename() == name;
    });
}

} // namespace

int main()
{
    const auto root = make_temp_dir();
    auto filesystem = create_platform_filesystem();

    const auto nested_dir = root / "nested" / "dir";
    require(filesystem.create_dir_all(nested_dir).has_value(), "create_dir_all failed");
    require(*filesystem.exists(nested_dir), "created directory does not exist");

    const auto path = nested_dir / "data.ldb";
    const FileOpenOptions create_options {
        .access = FileAccess::ReadWrite,
        .create_mode = FileCreateMode::CreateOrTruncate,
    };
    auto opened = filesystem.open(path, create_options);
    require(opened.has_value(), "open create failed");

    auto handle = std::move(*opened);
    const auto initial = bytes({1, 2, 3, 4});
    require(handle.write_at(0, initial).has_value(), "write_at failed");

    const std::byte overflow_byte {0};
    const auto overflow_write =
        handle.write_at(std::numeric_limits<std::uint64_t>::max(), std::span {&overflow_byte, 1});
    require(
        !overflow_write && overflow_write.error().is(FileSystemErrorCode::InvalidArgument),
        "write_at must reject offset plus length overflow"
    );
    std::array<std::byte, 1> overflow_buffer {};
    const auto overflow_read =
        handle.read_at(std::numeric_limits<std::uint64_t>::max(), overflow_buffer);
    require(
        !overflow_read && overflow_read.error().is(FileSystemErrorCode::InvalidArgument),
        "read_at must reject offset plus length overflow"
    );

    const auto patch = bytes({9, 8});
    require(handle.write_at(1, patch).has_value(), "second write_at failed");

    std::array<std::byte, 8> buffer {};
    const auto read = handle.read_at(0, buffer);
    require(read.has_value() && *read == 4, "read_at returned wrong byte count");
    require(
        buffer[0] == std::byte {1} && buffer[1] == std::byte {9} && buffer[2] == std::byte {8} &&
            buffer[3] == std::byte {4},
        "read_at returned wrong data"
    );

    const auto appended = bytes({5, 6});
    require(handle.append(appended).has_value(), "append failed");
    require(*handle.size() == 6, "append produced wrong file size");
    require(handle.truncate(3).has_value(), "truncate failed");
    require(*handle.size() == 3, "truncate produced wrong file size");
    require(handle.sync_all().has_value(), "sync_all failed");
    require(handle.close().has_value(), "close failed");
    require(handle.close().has_value(), "repeated close must succeed");

    const auto require_closed = [](const auto & result, const char * message) {
        require(!result && result.error().is(FileSystemErrorCode::ClosedHandle), message);
    };
    std::array<std::byte, 1> closed_buffer {};
    require_closed(handle.read_at(0, closed_buffer), "read after close must return ClosedHandle");
    require_closed(handle.write_at(0, initial), "write after close must return ClosedHandle");
    require_closed(handle.append(initial), "append after close must return ClosedHandle");
    require_closed(handle.size(), "size after close must return ClosedHandle");
    require_closed(handle.truncate(0), "truncate after close must return ClosedHandle");
    require_closed(handle.sync_data(), "sync_data after close must return ClosedHandle");
    require_closed(handle.sync_all(), "sync_all after close must return ClosedHandle");

    auto create_new_again = filesystem.open(
        path,
        FileOpenOptions {
            .access = FileAccess::ReadWrite,
            .create_mode = FileCreateMode::CreateNew,
        }
    );
    require(
        !create_new_again && create_new_again.error().is(FileSystemErrorCode::AlreadyExists),
        "CreateNew must fail for an existing file"
    );

    const auto mode_path = nested_dir / "open-modes.ldb";
    auto open_or_create = filesystem.open(
        mode_path,
        FileOpenOptions {
            .access = FileAccess::ReadWrite,
            .create_mode = FileCreateMode::OpenOrCreate,
        }
    );
    require(open_or_create.has_value(), "OpenOrCreate failed for a missing file");
    require(open_or_create->write_at(0, initial).has_value(), "OpenOrCreate write failed");
    require(open_or_create->close().has_value(), "OpenOrCreate close failed");

    auto reopened_without_truncate = filesystem.open(
        mode_path,
        FileOpenOptions {
            .access = FileAccess::ReadWrite,
            .create_mode = FileCreateMode::OpenOrCreate,
        }
    );
    require(
        reopened_without_truncate && *reopened_without_truncate->size() == initial.size(),
        "OpenOrCreate must preserve an existing file"
    );
    require(reopened_without_truncate->close().has_value(), "OpenOrCreate reopen close failed");

    auto truncate_existing = filesystem.open(
        mode_path,
        FileOpenOptions {
            .access = FileAccess::ReadWrite,
            .create_mode = FileCreateMode::TruncateExisting,
        }
    );
    require(
        truncate_existing && *truncate_existing->size() == 0,
        "TruncateExisting must clear an existing file"
    );
    require(truncate_existing->close().has_value(), "TruncateExisting close failed");

    const auto missing_truncate_path = nested_dir / "missing-truncate.ldb";
    auto missing_truncate = filesystem.open(
        missing_truncate_path,
        FileOpenOptions {
            .access = FileAccess::ReadWrite,
            .create_mode = FileCreateMode::TruncateExisting,
        }
    );
    require(
        !missing_truncate && missing_truncate.error().is(FileSystemErrorCode::NotFound),
        "TruncateExisting must reject a missing file"
    );
    require(filesystem.remove(mode_path).has_value(), "open mode fixture cleanup failed");

    auto existing = filesystem.open(
        path,
        FileOpenOptions {
            .access = FileAccess::ReadOnly,
            .create_mode = FileCreateMode::OpenExisting,
        }
    );
    require(existing.has_value(), "OpenExisting failed for an existing file");
    require(existing->close().has_value(), "readonly close failed");

    auto readonly = filesystem.open(
        path,
        FileOpenOptions {
            .access = FileAccess::ReadOnly,
            .create_mode = FileCreateMode::OpenExisting,
        }
    );
    require(readonly.has_value(), "failed to open read-only handle");
    const auto readonly_write = readonly->write_at(0, std::span {&overflow_byte, 1});
    require(
        !readonly_write && readonly_write.error().is(FileSystemErrorCode::PermissionDenied),
        "write through a read-only handle must return PermissionDenied"
    );
    require(readonly->close().has_value(), "failed to close read-only handle");

    auto writeonly = filesystem.open(
        path,
        FileOpenOptions {
            .access = FileAccess::WriteOnly,
            .create_mode = FileCreateMode::OpenExisting,
        }
    );
    require(writeonly.has_value(), "failed to open write-only handle");
    std::array<std::byte, 1> writeonly_buffer {};
    const auto writeonly_read = writeonly->read_at(0, writeonly_buffer);
    require(
        !writeonly_read && writeonly_read.error().is(FileSystemErrorCode::PermissionDenied),
        "read through a write-only handle must return PermissionDenied"
    );
    require(writeonly->close().has_value(), "failed to close write-only handle");

    const auto invalid_truncate = filesystem.open(
        path,
        FileOpenOptions {
            .access = FileAccess::ReadOnly,
            .create_mode = FileCreateMode::CreateOrTruncate,
        }
    );
    require(
        !invalid_truncate && invalid_truncate.error().is(FileSystemErrorCode::InvalidArgument),
        "read-only truncate must be rejected consistently"
    );
    const auto * invalid_context = invalid_truncate.error().context<FileSystemErrorContext>();
    require(
        invalid_context != nullptr && invalid_context->operation == "open" &&
            invalid_context->path == path,
        "invalid open error must preserve operation and path context"
    );

    const auto missing_path = nested_dir / "missing.ldb";
    const auto missing = filesystem.open(missing_path);
    require(!missing, "opening a missing file must fail");
    const auto * missing_context = missing.error().context<FileSystemErrorContext>();
    require(
        missing.error().is(FileSystemErrorCode::NotFound) && missing_context != nullptr &&
            !missing_context->operation.empty() && missing_context->path == missing_path &&
            static_cast<bool>(missing_context->native_code),
        "native open error must preserve operation, path, and native cause"
    );

    const auto entries = filesystem.list_dir(nested_dir);
    require(entries.has_value(), "list_dir failed");
    require(contains_filename(*entries, "data.ldb"), "list_dir did not include created file");
    require(
        std::ranges::all_of(
            *entries,
            [](const std::filesystem::path & entry) {
                return entry.parent_path().empty();
            }
        ),
        "list_dir must return entry names rather than full paths"
    );

    const auto renamed = nested_dir / "renamed.ldb";
    require(filesystem.rename(path, renamed).has_value(), "rename failed");
    require(!*filesystem.exists(path), "old path still exists after rename");
    require(*filesystem.exists(renamed), "renamed path missing");

    const auto occupied = nested_dir / "occupied.ldb";
    auto occupied_handle = filesystem.open(
        occupied,
        FileOpenOptions {
            .access = FileAccess::ReadWrite,
            .create_mode = FileCreateMode::CreateOrTruncate,
        }
    );
    require(occupied_handle.has_value(), "failed to create occupied rename target");
    const auto occupied_bytes = bytes({7});
    require(
        occupied_handle->write_at(0, occupied_bytes).has_value(),
        "failed to write occupied target"
    );
    require(occupied_handle->close().has_value(), "failed to close occupied target");

    const auto rename_over_existing = filesystem.rename(renamed, occupied);
    require(
        !rename_over_existing &&
            rename_over_existing.error().is(FileSystemErrorCode::AlreadyExists),
        "rename must not replace an existing destination"
    );
    require(*filesystem.exists(renamed), "failed rename removed the source");

    auto rename_filesystem = create_platform_filesystem();
    auto create_filesystem = create_platform_filesystem();
    constexpr std::size_t rename_race_count = 64;
    const std::array renamed_marker {std::byte {0xa1}};
    const std::array created_marker {std::byte {0xb2}};
    for (std::size_t iteration = 0; iteration < rename_race_count; ++iteration) {
        const auto race_source = nested_dir / ("rename-race-" + std::to_string(iteration) + ".src");
        const auto race_target = nested_dir / ("rename-race-" + std::to_string(iteration) + ".dst");
        auto source = filesystem.open(
            race_source,
            FileOpenOptions {
                .access = FileAccess::ReadWrite,
                .create_mode = FileCreateMode::CreateNew,
            }
        );
        require(source.has_value(), "failed to create rename race source");
        require(
            source->write_at(0, renamed_marker).has_value(),
            "failed to write rename race source"
        );
        require(source->close().has_value(), "failed to close rename race source");

        std::barrier start {3};
        bool rename_succeeded = false;
        bool rename_already_exists = false;
        bool rename_failed_unexpectedly = false;
        bool create_succeeded = false;
        bool create_already_exists = false;
        bool create_failed_unexpectedly = false;
        std::jthread renamer([&] {
            start.arrive_and_wait();
            auto result = rename_filesystem.rename(race_source, race_target);
            rename_succeeded = result.has_value();
            rename_already_exists =
                !result && result.error().is(FileSystemErrorCode::AlreadyExists);
            rename_failed_unexpectedly = !result && !rename_already_exists;
        });
        std::jthread creator([&] {
            start.arrive_and_wait();
            auto result = create_filesystem.open(
                race_target,
                FileOpenOptions {
                    .access = FileAccess::ReadWrite,
                    .create_mode = FileCreateMode::CreateNew,
                }
            );
            if (!result) {
                create_already_exists = result.error().is(FileSystemErrorCode::AlreadyExists);
                create_failed_unexpectedly = !create_already_exists;
                return;
            }
            create_succeeded =
                result->write_at(0, created_marker).has_value() && result->close().has_value();
            create_failed_unexpectedly = !create_succeeded;
        });
        start.arrive_and_wait();
        renamer.join();
        creator.join();

        require(
            !rename_failed_unexpectedly && !create_failed_unexpectedly,
            "rename race returned an unexpected error"
        );
        require(
            rename_succeeded != create_succeeded,
            "atomic no-replace rename and CreateNew must have exactly one winner"
        );
        require(
            (rename_succeeded && create_already_exists) ||
                (create_succeeded && rename_already_exists),
            "rename race loser must observe AlreadyExists"
        );

        auto target = filesystem.open(race_target);
        require(target.has_value(), "rename race target is missing");
        std::array<std::byte, 1> marker {};
        const auto marker_read = target->read_at(0, marker);
        require(marker_read && *marker_read == 1, "rename race target could not be read");
        require(
            marker == (rename_succeeded ? renamed_marker : created_marker),
            "rename race overwrote the winning target"
        );
        require(target->close().has_value(), "failed to close rename race target");
        require(filesystem.remove(race_source).has_value(), "rename race source cleanup failed");
        require(filesystem.remove(race_target).has_value(), "rename race target cleanup failed");
    }

    const auto replacement = nested_dir / "replacement.tmp";
    auto replacement_handle = filesystem.open(
        replacement,
        FileOpenOptions {
            .access = FileAccess::ReadWrite,
            .create_mode = FileCreateMode::CreateOrTruncate,
        }
    );
    require(replacement_handle.has_value(), "failed to create replacement file");
    const auto replacement_bytes = bytes({4, 2});
    require(
        replacement_handle->write_at(0, replacement_bytes).has_value(),
        "failed to write replacement file"
    );
    require(replacement_handle->sync_all().has_value(), "failed to sync replacement file");
    require(replacement_handle->close().has_value(), "failed to close replacement file");
    require(
        filesystem.replace_file_atomic(replacement, occupied).has_value(),
        "atomic replacement failed"
    );
    require(!*filesystem.exists(replacement), "replacement source still exists");

    auto replaced = filesystem.open(occupied);
    require(replaced.has_value(), "failed to open replaced file");
    std::array<std::byte, 4> replaced_buffer {};
    const auto replaced_read = replaced->read_at(0, replaced_buffer);
    require(
        replaced_read && *replaced_read == 2 && replaced_buffer[0] == std::byte {4} &&
            replaced_buffer[1] == std::byte {2},
        "atomic replacement published the wrong bytes"
    );
    require(replaced->close().has_value(), "failed to close replaced file");

    const auto concurrent_path = nested_dir / "concurrent-append.ldb";
    auto concurrent = filesystem.open(
        concurrent_path,
        FileOpenOptions {
            .access = FileAccess::ReadWrite,
            .create_mode = FileCreateMode::CreateOrTruncate,
        }
    );
    require(concurrent.has_value(), "failed to create concurrent append file");
    constexpr std::size_t append_count = 200;
    const std::array first_record {
        std::byte {0x11},
        std::byte {0x11},
        std::byte {0x11},
        std::byte {0x11},
        std::byte {0x11},
        std::byte {0x11},
        std::byte {0x11},
        std::byte {0x11},
    };
    const std::array second_record {
        std::byte {0x22},
        std::byte {0x22},
        std::byte {0x22},
        std::byte {0x22},
        std::byte {0x22},
        std::byte {0x22},
        std::byte {0x22},
        std::byte {0x22},
    };
    bool first_ok = true;
    bool second_ok = true;
    std::jthread first_thread([&] {
        for (std::size_t index = 0; index < append_count; ++index) {
            if (!concurrent->append(first_record)) {
                first_ok = false;
                return;
            }
        }
    });
    std::jthread second_thread([&] {
        for (std::size_t index = 0; index < append_count; ++index) {
            if (!concurrent->append(second_record)) {
                second_ok = false;
                return;
            }
        }
    });
    first_thread.join();
    second_thread.join();
    require(first_ok && second_ok, "same-handle concurrent append failed");
    require(
        *concurrent->size() == 2 * append_count * first_record.size(),
        "same-handle concurrent append lost bytes"
    );
    std::vector<std::byte> concurrent_bytes(2 * append_count * first_record.size());
    const auto concurrent_read = concurrent->read_at(0, concurrent_bytes);
    require(
        concurrent_read && *concurrent_read == concurrent_bytes.size(),
        "failed to read concurrent append result"
    );
    std::size_t first_records = 0;
    std::size_t second_records = 0;
    for (std::size_t offset = 0; offset < concurrent_bytes.size(); offset += first_record.size()) {
        const auto value = concurrent_bytes[offset];
        require(
            value == first_record.front() || value == second_record.front(),
            "concurrent append produced an unknown record"
        );
        for (std::size_t index = 1; index < first_record.size(); ++index) {
            require(
                concurrent_bytes[offset + index] == value,
                "same-handle append records were interleaved"
            );
        }
        if (value == first_record.front()) {
            ++first_records;
        } else {
            ++second_records;
        }
    }
    require(
        first_records == append_count && second_records == append_count,
        "same-handle concurrent append lost a complete record"
    );
    require(concurrent->close().has_value(), "failed to close concurrent append file");

    const auto sync_result = filesystem.sync_directory(nested_dir);
    require(
        sync_result.has_value() || sync_result.error().is(FileSystemErrorCode::Unsupported),
        "sync_directory returned an unexpected error"
    );

    require(filesystem.remove(renamed).has_value(), "remove file failed");
    require(filesystem.remove(occupied).has_value(), "remove replacement target failed");
    require(filesystem.remove(concurrent_path).has_value(), "remove concurrent append file failed");
    require(filesystem.remove(nested_dir).has_value(), "remove leaf directory failed");
    require(filesystem.remove(root / "nested").has_value(), "remove parent directory failed");
    require(filesystem.remove(root).has_value(), "remove root directory failed");
    require(!*filesystem.exists(root), "removed root still exists");
    require(filesystem.remove(root).has_value(), "remove must succeed for a missing path");
}

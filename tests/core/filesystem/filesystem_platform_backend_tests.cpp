#include "core/filesystem/platform_filesystem.hpp"

#include <algorithm>
#include <array>
#include <cstddef>
#include <filesystem>
#include <stdexcept>
#include <string>
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
    auto path = std::filesystem::temp_directory_path() / "litedb_filesystem_platform_backend_tests";
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
    require(filesystem.exists(nested_dir).value(), "created directory does not exist");

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

    const auto patch = bytes({9, 8});
    require(handle.write_at(1, patch).has_value(), "second write_at failed");

    std::array<std::byte, 8> buffer {};
    const auto read = handle.read_at(0, buffer);
    require(read.has_value() && *read == 4, "read_at returned wrong byte count");
    require(
        buffer[0] == std::byte {1} &&
            buffer[1] == std::byte {9} &&
            buffer[2] == std::byte {8} &&
            buffer[3] == std::byte {4},
        "read_at returned wrong data"
    );

    const auto appended = bytes({5, 6});
    require(handle.append(appended).has_value(), "append failed");
    require(handle.size().value() == 6, "append produced wrong file size");
    require(handle.truncate(3).has_value(), "truncate failed");
    require(handle.size().value() == 3, "truncate produced wrong file size");
    require(handle.sync_all().has_value(), "sync_all failed");
    require(handle.close().has_value(), "close failed");

    auto create_new_again = filesystem.open(
        path,
        FileOpenOptions {
            .access = FileAccess::ReadWrite,
            .create_mode = FileCreateMode::CreateNew,
        }
    );
    require(
        !create_new_again && create_new_again.error().code == FileSystemErrorCode::AlreadyExists,
        "CreateNew must fail for an existing file"
    );

    auto existing = filesystem.open(
        path,
        FileOpenOptions {
            .access = FileAccess::ReadOnly,
            .create_mode = FileCreateMode::OpenExisting,
        }
    );
    require(existing.has_value(), "OpenExisting failed for an existing file");
    require(existing->close().has_value(), "readonly close failed");

    const auto entries = filesystem.list_dir(nested_dir);
    require(entries.has_value(), "list_dir failed");
    require(contains_filename(*entries, "data.ldb"), "list_dir did not include created file");

    const auto renamed = nested_dir / "renamed.ldb";
    require(filesystem.rename(path, renamed).has_value(), "rename failed");
    require(!filesystem.exists(path).value(), "old path still exists after rename");
    require(filesystem.exists(renamed).value(), "renamed path missing");

    const auto sync_result = filesystem.sync_directory(nested_dir);
    require(
        sync_result.has_value() || sync_result.error().code == FileSystemErrorCode::Unsupported,
        "sync_directory returned an unexpected error"
    );

    require(filesystem.remove(renamed).has_value(), "remove file failed");
    require(filesystem.remove(nested_dir).has_value(), "remove leaf directory failed");
    require(filesystem.remove(root / "nested").has_value(), "remove parent directory failed");
    require(filesystem.remove(root).has_value(), "remove root directory failed");
    require(!filesystem.exists(root).value(), "removed root still exists");
}

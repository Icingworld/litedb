#include "core/filesystem/backend/filesystem_backend.hpp"
#include "core/filesystem/filesystem.hpp"

#include <algorithm>
#include <array>
#include <cstring>
#include <memory>
#include <stdexcept>
#include <type_traits>
#include <utility>
#include <vector>

namespace
{

using namespace litedb::core::filesystem;
using namespace litedb::core::filesystem::backend;
using litedb::core::error::Error;

void require(bool condition, const char * message)
{
    if (!condition) {
        throw std::runtime_error(message);
    }
}

template <typename T>
void require_filesystem_error(
    const std::expected<T, Error> & result,
    FileSystemErrorCode code,
    const char * operation
)
{
    require(!result && result.error().is(code), "unexpected filesystem error code");
    const auto * context = result.error().template context<FileSystemErrorContext>();
    require(
        context != nullptr && context->operation == operation,
        "filesystem error context did not preserve the operation"
    );
}

struct TestState
{
    std::vector<std::byte> data;
    std::filesystem::path last_path;
    std::filesystem::path rename_to;
    std::filesystem::path replace_to;
    FileOpenOptions last_options;
    bool data_synced {false};
    bool all_synced {false};
    bool directory_synced {false};
    bool directory_created {false};
    bool removed {false};
    bool replaced {false};
    int handle_destructions {0};
    int filesystem_destructions {0};
};

class MemoryFileHandleBackend final : public FileHandleBackend
{
public:
    explicit MemoryFileHandleBackend(std::shared_ptr<TestState> state)
        : state_(std::move(state))
    {}

    ~MemoryFileHandleBackend() override
    {
        ++state_->handle_destructions;
    }

    std::expected<void, Error> close() override
    {
        return {};
    }

    std::expected<std::size_t, Error>
    read_at(std::uint64_t offset, std::span<std::byte> buffer) override
    {
        if (offset >= state_->data.size()) {
            return 0;
        }

        const auto count =
            std::min(buffer.size(), state_->data.size() - static_cast<std::size_t>(offset));
        std::memcpy(buffer.data(), state_->data.data() + offset, count);
        return count;
    }

    std::expected<void, Error>
    write_at(std::uint64_t offset, std::span<const std::byte> data) override
    {
        const auto end = static_cast<std::size_t>(offset) + data.size();
        if (end > state_->data.size()) {
            state_->data.resize(end);
        }
        std::memcpy(state_->data.data() + offset, data.data(), data.size());
        return {};
    }

    std::expected<void, Error> append(std::span<const std::byte> data) override
    {
        state_->data.insert(state_->data.end(), data.begin(), data.end());
        return {};
    }

    std::expected<std::uint64_t, Error> size() override
    {
        return state_->data.size();
    }

    std::expected<void, Error> truncate(std::uint64_t size) override
    {
        state_->data.resize(static_cast<std::size_t>(size));
        return {};
    }

    std::expected<void, Error> sync_data() override
    {
        state_->data_synced = true;
        return {};
    }

    std::expected<void, Error> sync_all() override
    {
        state_->all_synced = true;
        return {};
    }

private:
    std::shared_ptr<TestState> state_;
};

class MemoryFileSystemBackend final : public FileSystemBackend
{
public:
    explicit MemoryFileSystemBackend(std::shared_ptr<TestState> state)
        : state_(std::move(state))
    {}

    ~MemoryFileSystemBackend() override
    {
        ++state_->filesystem_destructions;
    }

    std::expected<std::unique_ptr<FileHandleBackend>, Error>
    open(const std::filesystem::path & path, const FileOpenOptions & options) override
    {
        state_->last_path = path;
        state_->last_options = options;
        if (path == "missing") {
            return std::unexpected(Error {FileSystemErrorCode::NotFound, "missing"});
        }

        std::unique_ptr<FileHandleBackend> handle =
            std::make_unique<MemoryFileHandleBackend>(state_);
        return handle;
    }

    std::expected<std::vector<std::filesystem::path>, Error> list_dir(
        const std::filesystem::path & path
    ) override
    {
        state_->last_path = path;
        return std::vector<std::filesystem::path> {"first", "second"};
    }

    std::expected<bool, Error> exists(const std::filesystem::path & path) override
    {
        state_->last_path = path;
        return path != "missing";
    }

    std::expected<void, Error> create_dir_all(const std::filesystem::path & path) override
    {
        state_->last_path = path;
        state_->directory_created = true;
        return {};
    }

    std::expected<void, Error>
    rename(const std::filesystem::path & from, const std::filesystem::path & to) override
    {
        state_->last_path = from;
        state_->rename_to = to;
        return {};
    }

    std::expected<void, Error> replace_file_atomic(
        const std::filesystem::path & from,
        const std::filesystem::path & to
    ) override
    {
        state_->last_path = from;
        state_->replace_to = to;
        state_->replaced = true;
        return {};
    }

    std::expected<void, Error> remove(const std::filesystem::path & path) override
    {
        state_->last_path = path;
        state_->removed = true;
        return {};
    }

    std::expected<void, Error> sync_directory(const std::filesystem::path & path) override
    {
        state_->last_path = path;
        state_->directory_synced = true;
        return {};
    }

private:
    std::shared_ptr<TestState> state_;
};

} // namespace

int main()
{
    static_assert(!std::is_abstract_v<FileSystem>);
    static_assert(!std::is_abstract_v<FileHandle>);
    static_assert(!std::is_copy_constructible_v<FileSystem>);
    static_assert(std::is_nothrow_move_constructible_v<FileSystem>);
    static_assert(!std::is_copy_constructible_v<FileHandle>);
    static_assert(std::is_nothrow_move_constructible_v<FileHandle>);

    {
        FileSystem empty_filesystem {std::unique_ptr<FileSystemBackend> {}};
        require_filesystem_error(
            empty_filesystem.open("empty.ldb"),
            FileSystemErrorCode::InvalidState,
            "open"
        );

        FileHandle empty_handle {std::unique_ptr<FileHandleBackend> {}};
        require_filesystem_error(empty_handle.close(), FileSystemErrorCode::InvalidState, "close");
    }

    auto state = std::make_shared<TestState>();
    {
        FileSystem filesystem {std::make_unique<MemoryFileSystemBackend>(state)};
        auto moved_filesystem = std::move(filesystem);

        require_filesystem_error(
            filesystem.open("moved.ldb"),
            FileSystemErrorCode::InvalidState,
            "open"
        );
        require_filesystem_error(
            filesystem.list_dir("root"),
            FileSystemErrorCode::InvalidState,
            "list_dir"
        );
        require_filesystem_error(
            filesystem.exists("moved.ldb"),
            FileSystemErrorCode::InvalidState,
            "exists"
        );
        require_filesystem_error(
            filesystem.create_dir_all("root"),
            FileSystemErrorCode::InvalidState,
            "create_dir_all"
        );
        require_filesystem_error(
            filesystem.rename("old", "new"),
            FileSystemErrorCode::InvalidState,
            "rename"
        );
        require_filesystem_error(
            filesystem.replace_file_atomic("temporary", "published"),
            FileSystemErrorCode::InvalidState,
            "replace_file_atomic"
        );
        require_filesystem_error(
            filesystem.remove("moved.ldb"),
            FileSystemErrorCode::InvalidState,
            "remove"
        );
        require_filesystem_error(
            filesystem.sync_directory("root"),
            FileSystemErrorCode::InvalidState,
            "sync_directory"
        );

        auto * filesystem_alias = &moved_filesystem;
        moved_filesystem = std::move(*filesystem_alias);

        const FileOpenOptions options {
            .access = FileAccess::ReadWrite,
            .create_mode = FileCreateMode::OpenOrCreate,
        };
        auto opened = moved_filesystem.open("data.ldb", options);
        require(opened.has_value(), "open failed");
        require(state->last_path == "data.ldb", "open did not forward path");
        require(state->last_options.access == FileAccess::ReadWrite, "open did not forward access");
        require(
            state->last_options.create_mode == FileCreateMode::OpenOrCreate,
            "open did not forward create mode"
        );

        FileHandle file = std::move(*opened);
        FileHandle moved_file = std::move(file);
        std::array<std::byte, 1> moved_buffer {};
        const std::array moved_data {std::byte {0}};
        require_filesystem_error(file.close(), FileSystemErrorCode::InvalidState, "close");
        require_filesystem_error(
            file.read_at(0, moved_buffer),
            FileSystemErrorCode::InvalidState,
            "read_at"
        );
        require_filesystem_error(
            file.write_at(0, moved_data),
            FileSystemErrorCode::InvalidState,
            "write_at"
        );
        require_filesystem_error(
            file.append(moved_data),
            FileSystemErrorCode::InvalidState,
            "append"
        );
        require_filesystem_error(file.size(), FileSystemErrorCode::InvalidState, "size");
        require_filesystem_error(file.truncate(0), FileSystemErrorCode::InvalidState, "truncate");
        require_filesystem_error(file.sync_data(), FileSystemErrorCode::InvalidState, "sync_data");
        require_filesystem_error(file.sync_all(), FileSystemErrorCode::InvalidState, "sync_all");

        auto * file_alias = &moved_file;
        moved_file = std::move(*file_alias);
        const std::byte initial[] {std::byte {1}, std::byte {2}, std::byte {3}};
        require(moved_file.write_at(0, initial).has_value(), "write_at failed");

        std::byte buffer[5] {};
        const auto read = moved_file.read_at(1, buffer);
        require(read.has_value() && *read == 2, "read_at must report a short read at EOF");
        require(
            buffer[0] == std::byte {2} && buffer[1] == std::byte {3},
            "read_at returned wrong data"
        );

        const std::byte appended[] {std::byte {4}};
        require(moved_file.append(appended).has_value(), "append failed");
        require(*moved_file.size() == 4, "append produced wrong file size");
        require(moved_file.truncate(2).has_value(), "truncate failed");
        require(*moved_file.size() == 2, "truncate produced wrong file size");
        require(moved_file.sync_data().has_value() && state->data_synced, "sync_data failed");
        require(moved_file.sync_all().has_value() && state->all_synced, "sync_all failed");
        require(moved_file.close().has_value(), "close failed");
        require(moved_file.close().has_value(), "repeated close must succeed");

        const auto missing = moved_filesystem.open("missing");
        require(
            !missing && missing.error().is(FileSystemErrorCode::NotFound),
            "open did not propagate backend error"
        );

        const auto invalid_open = moved_filesystem.open(
            "data.ldb",
            FileOpenOptions {
                .access = FileAccess::ReadOnly,
                .create_mode = FileCreateMode::CreateOrTruncate,
            }
        );
        require(
            !invalid_open && invalid_open.error().is(FileSystemErrorCode::InvalidArgument),
            "read-only truncate must be rejected before reaching the backend"
        );

        const auto entries = moved_filesystem.list_dir("root");
        require(
            entries && entries->size() == 2 && state->last_path == "root",
            "list_dir forwarding failed"
        );
        require(*moved_filesystem.exists("present"), "exists forwarding failed");
        require(moved_filesystem.create_dir_all("new-dir").has_value(), "create_dir_all failed");
        require(
            state->directory_created && state->last_path == "new-dir",
            "create_dir_all forwarding failed"
        );
        require(moved_filesystem.rename("old", "new").has_value(), "rename failed");
        require(state->last_path == "old" && state->rename_to == "new", "rename forwarding failed");
        require(
            moved_filesystem.replace_file_atomic("temporary", "published").has_value(),
            "replace_file_atomic failed"
        );
        require(
            state->replaced && state->last_path == "temporary" && state->replace_to == "published",
            "replace_file_atomic forwarding failed"
        );
        require(moved_filesystem.remove("obsolete").has_value(), "remove failed");
        require(state->removed && state->last_path == "obsolete", "remove forwarding failed");
        require(moved_filesystem.sync_directory("root").has_value(), "sync_directory failed");
        require(
            state->directory_synced && state->last_path == "root",
            "sync_directory forwarding failed"
        );
    }

    require(state->handle_destructions == 1, "file backend must be destroyed exactly once");
    require(
        state->filesystem_destructions == 1,
        "filesystem backend must be destroyed exactly once"
    );
}

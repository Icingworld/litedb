#include "core/filesystem/backend/filesystem_backend.hpp"
#include "core/filesystem/filesystem.hpp"

#include <algorithm>
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

void require(bool condition, const char * message)
{
    if (!condition) {
        throw std::runtime_error(message);
    }
}

struct TestState
{
    std::vector<std::byte> data;
    std::filesystem::path last_path;
    std::filesystem::path rename_to;
    FileOpenOptions last_options;
    bool data_synced {false};
    bool all_synced {false};
    bool directory_synced {false};
    bool directory_created {false};
    bool removed {false};
    int handle_destructions {0};
    int filesystem_destructions {0};
};

class MemoryFileHandleBackend final : public FileHandleBackend
{
public:
    explicit MemoryFileHandleBackend(std::shared_ptr<TestState> state)
        : state_(std::move(state))
    {
    }

    ~MemoryFileHandleBackend() override
    {
        ++state_->handle_destructions;
    }

    std::expected<std::size_t, FileSystemError> read_at(
        std::uint64_t offset,
        std::span<std::byte> buffer
    ) override
    {
        if (offset >= state_->data.size()) {
            return 0;
        }

        const auto count = std::min(buffer.size(), state_->data.size() - static_cast<std::size_t>(offset));
        std::memcpy(buffer.data(), state_->data.data() + offset, count);
        return count;
    }

    std::expected<void, FileSystemError> write_at(
        std::uint64_t offset,
        std::span<const std::byte> data
    ) override
    {
        const auto end = static_cast<std::size_t>(offset) + data.size();
        if (end > state_->data.size()) {
            state_->data.resize(end);
        }
        std::memcpy(state_->data.data() + offset, data.data(), data.size());
        return {};
    }

    std::expected<void, FileSystemError> append(std::span<const std::byte> data) override
    {
        state_->data.insert(state_->data.end(), data.begin(), data.end());
        return {};
    }

    std::expected<std::uint64_t, FileSystemError> size() override
    {
        return state_->data.size();
    }

    std::expected<void, FileSystemError> truncate(std::uint64_t size) override
    {
        state_->data.resize(static_cast<std::size_t>(size));
        return {};
    }

    std::expected<void, FileSystemError> sync_data() override
    {
        state_->data_synced = true;
        return {};
    }

    std::expected<void, FileSystemError> sync_all() override
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
    {
    }

    ~MemoryFileSystemBackend() override
    {
        ++state_->filesystem_destructions;
    }

    std::expected<std::unique_ptr<FileHandleBackend>, FileSystemError> open(
        const std::filesystem::path & path,
        const FileOpenOptions & options
    ) override
    {
        state_->last_path = path;
        state_->last_options = options;
        if (path == "missing") {
            return std::unexpected(FileSystemError {FileSystemErrorCode::NotFound, "missing"});
        }

        std::unique_ptr<FileHandleBackend> handle =
            std::make_unique<MemoryFileHandleBackend>(state_);
        return handle;
    }

    std::expected<std::vector<std::filesystem::path>, FileSystemError> list_dir(
        const std::filesystem::path & path
    ) override
    {
        state_->last_path = path;
        return std::vector<std::filesystem::path> {"first", "second"};
    }

    std::expected<bool, FileSystemError> exists(const std::filesystem::path & path) override
    {
        state_->last_path = path;
        return path != "missing";
    }

    std::expected<void, FileSystemError> create_dir(const std::filesystem::path & path) override
    {
        state_->last_path = path;
        state_->directory_created = true;
        return {};
    }

    std::expected<void, FileSystemError> rename(
        const std::filesystem::path & from,
        const std::filesystem::path & to
    ) override
    {
        state_->last_path = from;
        state_->rename_to = to;
        return {};
    }

    std::expected<void, FileSystemError> remove(const std::filesystem::path & path) override
    {
        state_->last_path = path;
        state_->removed = true;
        return {};
    }

    std::expected<void, FileSystemError> sync_directory(const std::filesystem::path & path) override
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

    auto state = std::make_shared<TestState>();
    {
        FileSystem filesystem {std::make_unique<MemoryFileSystemBackend>(state)};
        auto moved_filesystem = std::move(filesystem);

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
        const std::byte initial[] {std::byte {1}, std::byte {2}, std::byte {3}};
        require(file.write_at(0, initial).has_value(), "write_at failed");

        std::byte buffer[5] {};
        const auto read = file.read_at(1, buffer);
        require(read.has_value() && *read == 2, "read_at must report a short read at EOF");
        require(buffer[0] == std::byte {2} && buffer[1] == std::byte {3}, "read_at returned wrong data");

        const std::byte appended[] {std::byte {4}};
        require(file.append(appended).has_value(), "append failed");
        require(file.size().value() == 4, "append produced wrong file size");
        require(file.truncate(2).has_value(), "truncate failed");
        require(file.size().value() == 2, "truncate produced wrong file size");
        require(file.sync_data().has_value() && state->data_synced, "sync_data failed");
        require(file.sync_all().has_value() && state->all_synced, "sync_all failed");

        const auto missing = moved_filesystem.open("missing");
        require(
            !missing && missing.error().code == FileSystemErrorCode::NotFound,
            "open did not propagate backend error"
        );

        const auto entries = moved_filesystem.list_dir("root");
        require(entries && entries->size() == 2 && state->last_path == "root", "list_dir forwarding failed");
        require(moved_filesystem.exists("present").value(), "exists forwarding failed");
        require(moved_filesystem.create_dir("new-dir").has_value(), "create_dir failed");
        require(state->directory_created && state->last_path == "new-dir", "create_dir forwarding failed");
        require(moved_filesystem.rename("old", "new").has_value(), "rename failed");
        require(state->last_path == "old" && state->rename_to == "new", "rename forwarding failed");
        require(moved_filesystem.remove("obsolete").has_value(), "remove failed");
        require(state->removed && state->last_path == "obsolete", "remove forwarding failed");
        require(moved_filesystem.sync_directory("root").has_value(), "sync_directory failed");
        require(
            state->directory_synced && state->last_path == "root",
            "sync_directory forwarding failed"
        );
    }

    require(state->handle_destructions == 1, "file backend must be destroyed exactly once");
    require(state->filesystem_destructions == 1, "filesystem backend must be destroyed exactly once");
}

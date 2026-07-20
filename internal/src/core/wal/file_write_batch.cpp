#include "core/wal/file_write_batch.hpp"

#include <algorithm>
#include <limits>
#include <unordered_map>
#include <utility>

#include "core/filesystem/backend/filesystem_backend.hpp"

namespace litedb::core::wal
{
namespace
{
WalError filesystem_error(filesystem::FileSystemError error)
{
    return WalError {.code = WalErrorCode::FileSystemError, .message = std::move(error.message)};
}
} // namespace

void FileWriteBatch::add(FileWrite write)
{
    writes_.push_back(std::move(write));
}

std::expected<std::vector<std::byte>, WalError> FileWriteBatch::read(
    const FileTarget & target,
    std::uint64_t offset,
    std::span<const std::byte> base
) const
{
    std::vector<std::byte> result(base.begin(), base.end());
    const auto end = offset + result.size();
    if (end < offset) {
        return std::unexpected(WalError {WalErrorCode::InvalidRecord, "FileWriteBatch read range overflows"});
    }
    for (const auto & write : writes_) {
        if (write.target != target) continue;
        const auto write_end = write.offset + write.after_image.size();
        if (write_end < write.offset || write.offset >= end || write_end <= offset) continue;
        const auto overlap_begin = std::max(offset, write.offset);
        const auto overlap_end = std::min(end, write_end);
        std::copy(
            write.after_image.begin() + static_cast<std::ptrdiff_t>(overlap_begin - write.offset),
            write.after_image.begin() + static_cast<std::ptrdiff_t>(overlap_end - write.offset),
            result.begin() + static_cast<std::ptrdiff_t>(overlap_begin - offset)
        );
    }
    return result;
}

std::filesystem::path FileWriteBatch::resolve_target(
    const std::filesystem::path & data_directory,
    const FileTarget & target
)
{
    switch (target.kind) {
    case FileKind::CollectionStore:
        return data_directory / "collections" / (std::to_string(target.object_id) + ".store");
    case FileKind::ScalarIndex:
        return data_directory / "indexes" / (std::to_string(target.object_id) + ".bti");
    case FileKind::VectorIndex:
        return data_directory / "vindexes" / ("vindex_" + std::to_string(target.object_id) + ".lhnsw");
    }
    return {};
}

std::expected<void, WalError> FileWriteBatch::apply(
    const std::filesystem::path & data_directory,
    filesystem::FileSystem & filesystem,
    bool sync
) const
{
    std::vector<std::filesystem::path> touched;
    for (const auto & write : writes_) {
        const auto path = resolve_target(data_directory, write.target);
        auto exists = filesystem.exists(path);
        if (!exists) return std::unexpected(filesystem_error(std::move(exists.error())));
        if (!*exists) {
            return std::unexpected(WalError {WalErrorCode::MissingTarget, "Committed WAL target is missing: " + path.string()});
        }
        auto file = filesystem.open(path, filesystem::backend::FileOpenOptions {
            .access = filesystem::backend::FileAccess::ReadWrite,
            .create_mode = filesystem::backend::FileCreateMode::OpenExisting,
        });
        if (!file) return std::unexpected(filesystem_error(std::move(file.error())));
        auto written = file->write_at(write.offset, write.after_image);
        if (!written) return std::unexpected(filesystem_error(std::move(written.error())));
        if (sync) {
            auto synced = file->sync_data();
            if (!synced) return std::unexpected(filesystem_error(std::move(synced.error())));
        }
        touched.push_back(path);
    }
    return {};
}

} // namespace litedb::core::wal

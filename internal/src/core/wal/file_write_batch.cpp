#include "core/wal/file_write_batch.hpp"

#include <algorithm>
#include <utility>

#include "core/filesystem/backend/filesystem_backend.hpp"

namespace litedb::core::wal
{

namespace
{

/**
 * @brief 从文件系统错误创建 WAL 错误
 * @param value 文件系统错误
 * @return WAL 错误
 */
[[nodiscard]]
WalError fs_error(filesystem::FileSystemError value)
{
    return make_error(WalErrorCode::FileSystemError, std::move(value.message));
}

} // namespace

void FileWriteBatch::add(FileWrite write)
{
    writes_.push_back(std::move(write));
}

const std::vector<FileWrite> & FileWriteBatch::writes() const noexcept
{
    return writes_;
}

bool FileWriteBatch::empty() const noexcept
{
    return writes_.empty();
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
        return std::unexpected(make_error(WalErrorCode::InvalidRecord, "FileWriteBatch read range overflows"));
    }

    for (const auto & write : writes_) {
        if (write.target != target) {
            continue;
        }
        if (write.mode == FileWriteMode::Delete) {
            return std::unexpected(make_error(WalErrorCode::MissingTarget, "WAL overlay target was deleted"));
        }

        const auto write_end = write.offset + write.after_image.size();
        if (write_end < write.offset || write.offset >= end || write_end <= offset) {
            continue;
        }

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
    case FileKind::MetaStore:
        return data_directory / "meta.lmeta";
    }
    return {};
}

std::expected<void, WalError> FileWriteBatch::apply(
    const std::filesystem::path & data_directory,
    filesystem::FileSystem & filesystem,
    bool sync
) const
{
    for (const auto & write : writes_) {
        const auto path = resolve_target(data_directory, write.target);
        auto exists = filesystem.exists(path);
        if (!exists) {
            return std::unexpected(fs_error(std::move(exists.error())));
        }
        if (write.mode == FileWriteMode::Delete) {
            if (*exists) {
                auto removed = filesystem.remove(path);
                if (!removed) return std::unexpected(fs_error(std::move(removed.error())));
            }
            continue;
        }

        auto parent_created = filesystem.create_dir_all(path.parent_path());
        if (!parent_created) return std::unexpected(fs_error(std::move(parent_created.error())));

        auto file = filesystem.open(
            path,
            {
                filesystem::FileAccess::ReadWrite,
                write.mode == FileWriteMode::Replace
                    ? filesystem::FileCreateMode::CreateOrTruncate
                    : filesystem::FileCreateMode::OpenOrCreate,
            }
        );
        if (!file) {
            return std::unexpected(fs_error(std::move(file.error())));
        }

        auto written = file->write_at(write.offset, write.after_image);
        if (!written) {
            return std::unexpected(fs_error(std::move(written.error())));
        }
        if (write.mode == FileWriteMode::Replace) {
            auto truncated = file->truncate(write.after_image.size());
            if (!truncated) return std::unexpected(fs_error(std::move(truncated.error())));
        }
        if (sync) {
            auto synced = file->sync_data();
            if (!synced) {
                return std::unexpected(fs_error(std::move(synced.error())));
            }
        }
    }
    return {};
}

} // namespace litedb::core::wal

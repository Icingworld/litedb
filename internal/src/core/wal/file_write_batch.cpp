#include "core/wal/file_write_batch.hpp"

#include <algorithm>
#include <limits>
#include <tuple>
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
WalError fs_error(error::Error value)
{
    return make_error(WalErrorCode::FileSystemError, value.message());
}

[[nodiscard]]
auto target_key(const FileTarget & target)
{
    return std::tuple {static_cast<std::uint8_t>(target.kind), target.object_id};
}

[[nodiscard]]
std::expected<std::vector<FileWrite>, WalError> normalize_writes(
    const std::vector<FileWrite> & source
)
{
    std::vector<FileWrite> writes = source;
    for (const auto & write : writes) {
        const auto mode = static_cast<std::uint8_t>(write.mode);
        const auto kind = static_cast<std::uint8_t>(write.target.kind);
        if (kind < static_cast<std::uint8_t>(FileKind::CollectionStore) ||
            kind > static_cast<std::uint8_t>(FileKind::MetaStore) ||
            (write.target.kind == FileKind::MetaStore ? write.target.object_id != 0
                                                       : write.target.object_id == 0) ||
            mode > static_cast<std::uint8_t>(FileWriteMode::Truncate)) {
            return std::unexpected(make_error(WalErrorCode::InvalidRecord, "Invalid file write target or mode"));
        }
        if ((write.mode == FileWriteMode::Delete || write.mode == FileWriteMode::Truncate) &&
            !write.after_image.empty()) {
            return std::unexpected(make_error(WalErrorCode::InvalidRecord, "File lifecycle write has an after-image"));
        }
        if (write.mode == FileWriteMode::Delete && write.offset != 0) {
            return std::unexpected(make_error(WalErrorCode::InvalidRecord, "Delete write has a non-zero offset"));
        }
        if (write.mode == FileWriteMode::Replace && write.offset != 0) {
            return std::unexpected(make_error(WalErrorCode::InvalidRecord, "Replace write has a non-zero offset"));
        }
        if ((write.mode == FileWriteMode::Overwrite || write.mode == FileWriteMode::Replace) &&
            write.after_image.size() > std::numeric_limits<std::uint64_t>::max() - write.offset) {
            return std::unexpected(make_error(WalErrorCode::InvalidRecord, "File write range overflows"));
        }
    }

    auto rank = [](FileWriteMode mode) {
        return mode == FileWriteMode::Truncate ? 1 : 0;
    };
    std::sort(writes.begin(), writes.end(), [&](const FileWrite & left, const FileWrite & right) {
        if (target_key(left.target) != target_key(right.target)) {
            return target_key(left.target) < target_key(right.target);
        }
        if (rank(left.mode) != rank(right.mode)) return rank(left.mode) < rank(right.mode);
        return left.offset < right.offset;
    });

    std::vector<FileWrite> result;
    for (std::size_t begin = 0; begin < writes.size();) {
        std::size_t end = begin + 1;
        while (end < writes.size() && writes[end].target == writes[begin].target) ++end;

        std::size_t deletes {0};
        std::size_t replaces {0};
        std::size_t truncates {0};
        std::optional<std::uint64_t> final_size;
        for (std::size_t index = begin; index < end; ++index) {
            deletes += writes[index].mode == FileWriteMode::Delete;
            replaces += writes[index].mode == FileWriteMode::Replace;
            truncates += writes[index].mode == FileWriteMode::Truncate;
            if (writes[index].mode == FileWriteMode::Truncate) final_size = writes[index].offset;
        }
        if (deletes != 0 || replaces != 0) {
            if (end - begin != 1) {
                return std::unexpected(make_error(WalErrorCode::InvalidRecord, "Conflicting lifecycle writes for one target"));
            }
            result.push_back(std::move(writes[begin]));
            begin = end;
            continue;
        }
        if (truncates > 1) {
            return std::unexpected(make_error(WalErrorCode::InvalidRecord, "Multiple truncate writes for one target"));
        }

        std::uint64_t previous_end {0};
        bool have_range {false};
        for (std::size_t index = begin; index < end; ++index) {
            auto & write = writes[index];
            if (write.mode == FileWriteMode::Truncate) continue;
            const auto range_end = write.offset + write.after_image.size();
            if (have_range && write.offset < previous_end) {
                return std::unexpected(make_error(WalErrorCode::InvalidRecord, "Overlapping file write ranges"));
            }
            if (final_size && range_end > *final_size) {
                return std::unexpected(make_error(WalErrorCode::InvalidRecord, "File write exceeds final truncated size"));
            }
            if (!result.empty() && result.back().target == write.target &&
                result.back().mode == FileWriteMode::Overwrite &&
                result.back().offset + result.back().after_image.size() == write.offset) {
                result.back().after_image.insert(
                    result.back().after_image.end(),
                    write.after_image.begin(),
                    write.after_image.end()
                );
            } else {
                result.push_back(std::move(write));
            }
            previous_end = range_end;
            have_range = true;
        }
        if (truncates == 1) {
            result.push_back(std::move(writes[end - 1]));
        }
        begin = end;
    }
    return result;
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
    bool sync,
    const FileWriteAppliedHook & applied_hook
) const
{
    auto normalized = normalize_writes(writes_);
    if (!normalized) return std::unexpected(std::move(normalized.error()));

    std::optional<FileTarget> current_target;
    std::optional<filesystem::FileHandle> current_file;
    auto sync_current = [&]() -> std::expected<void, WalError> {
        if (!sync || !current_file) return {};
        auto synced = current_file->sync_data();
        if (!synced) return std::unexpected(fs_error(std::move(synced.error())));
        return {};
    };

    std::size_t applied_count {0};
    for (const auto & write : *normalized) {
        const auto path = resolve_target(data_directory, write.target);
        if (current_target && *current_target != write.target) {
            auto synced = sync_current();
            if (!synced) return synced;
            current_file.reset();
        }
        current_target = write.target;
        auto exists = filesystem.exists(path);
        if (!exists) {
            return std::unexpected(fs_error(std::move(exists.error())));
        }
        if (write.mode == FileWriteMode::Delete) {
            current_file.reset();
            if (*exists) {
                auto removed = filesystem.remove(path);
                if (!removed) return std::unexpected(fs_error(std::move(removed.error())));
            }
            ++applied_count;
            if (applied_hook && applied_hook(applied_count, write)) {
                return std::unexpected(make_error(WalErrorCode::FileSystemError, "File write apply interrupted"));
            }
            continue;
        }

        auto parent_created = filesystem.create_dir_all(path.parent_path());
        if (!parent_created) return std::unexpected(fs_error(std::move(parent_created.error())));

        if (!current_file) {
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
            current_file.emplace(std::move(*file));
        }

        if (write.mode == FileWriteMode::Truncate) {
            auto truncated = current_file->truncate(write.offset);
            if (!truncated) return std::unexpected(fs_error(std::move(truncated.error())));
        } else {
            auto written = current_file->write_at(write.offset, write.after_image);
            if (!written) {
                return std::unexpected(fs_error(std::move(written.error())));
            }
        }
        if (write.mode == FileWriteMode::Replace) {
            auto truncated = current_file->truncate(write.after_image.size());
            if (!truncated) return std::unexpected(fs_error(std::move(truncated.error())));
        }
        ++applied_count;
        if (applied_hook && applied_hook(applied_count, write)) {
            return std::unexpected(make_error(WalErrorCode::FileSystemError, "File write apply interrupted"));
        }
    }
    return sync_current();
}

} // namespace litedb::core::wal

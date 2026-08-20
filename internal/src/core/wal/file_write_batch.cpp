#include "core/wal/file_write_batch.hpp"

#include <algorithm>
#include <limits>
#include <optional>
#include <tuple>
#include <utility>

namespace litedb::core::wal
{

namespace
{

// 生成排序和冲突检测使用的文件目标稳定键。
[[nodiscard]]
auto target_key(const FileTarget & target)
{
    return std::tuple {static_cast<std::uint8_t>(target.kind), target.object_id};
}

// 校验、排序、合并并规范化同一事务的文件写集合。
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
            kind > static_cast<std::uint8_t>(FileKind::CatalogStore) ||
            (write.target.kind == FileKind::CatalogStore ? write.target.object_id != 0
                                                         : write.target.object_id == 0) ||
            mode > static_cast<std::uint8_t>(FileWriteMode::Truncate)) [[unlikely]] {
            return std::unexpected(
                make_error(WalErrorCode::InvalidRecord, "Invalid file write target or mode")
            );
        }
        if ((write.mode == FileWriteMode::Delete || write.mode == FileWriteMode::Truncate) &&
            !write.after_image.empty()) [[unlikely]] {
            return std::unexpected(
                make_error(WalErrorCode::InvalidRecord, "File lifecycle write has an after-image")
            );
        }
        if ((write.mode == FileWriteMode::Delete || write.mode == FileWriteMode::Replace) &&
            write.offset != 0) [[unlikely]] {
            return std::unexpected(make_error(
                WalErrorCode::InvalidRecord,
                "File lifecycle write has a non-zero offset"
            ));
        }
        if ((write.mode == FileWriteMode::Overwrite || write.mode == FileWriteMode::Replace) &&
            write.offset > std::numeric_limits<std::uint64_t>::max() - write.after_image.size())
            [[unlikely]] {
            return std::unexpected(
                make_error(WalErrorCode::InvalidRecord, "File write range overflows")
            );
        }
    }

    auto rank = [](FileWriteMode mode) {
        return mode == FileWriteMode::Truncate ? 1 : 0;
    };
    std::sort(writes.begin(), writes.end(), [&](const FileWrite & left, const FileWrite & right) {
        if (target_key(left.target) != target_key(right.target)) {
            return target_key(left.target) < target_key(right.target);
        }
        if (rank(left.mode) != rank(right.mode)) {
            return rank(left.mode) < rank(right.mode);
        }
        return left.offset < right.offset;
    });

    std::vector<FileWrite> result;
    for (std::size_t begin = 0; begin < writes.size();) {
        std::size_t end = begin + 1;
        while (end < writes.size() && writes[end].target == writes[begin].target) {
            ++end;
        }
        std::size_t deletes {0};
        std::size_t replaces {0};
        std::size_t truncates {0};
        std::optional<std::uint64_t> final_size;
        for (std::size_t index = begin; index < end; ++index) {
            deletes += writes[index].mode == FileWriteMode::Delete;
            replaces += writes[index].mode == FileWriteMode::Replace;
            truncates += writes[index].mode == FileWriteMode::Truncate;
            if (writes[index].mode == FileWriteMode::Truncate) {
                final_size = writes[index].offset;
            }
        }
        if (deletes != 0 || replaces != 0) {
            if (end - begin != 1) [[unlikely]] {
                return std::unexpected(make_error(
                    WalErrorCode::InvalidRecord,
                    "Conflicting lifecycle writes for one target"
                ));
            }
            result.push_back(std::move(writes[begin]));
            begin = end;
            continue;
        }
        if (truncates > 1) [[unlikely]] {
            return std::unexpected(
                make_error(WalErrorCode::InvalidRecord, "Multiple truncate writes for one target")
            );
        }

        std::uint64_t previous_end {0};
        bool have_range {false};
        for (std::size_t index = begin; index < end; ++index) {
            auto & write = writes[index];
            if (write.mode == FileWriteMode::Truncate) {
                continue;
            }
            const auto range_end = write.offset + write.after_image.size();
            if (have_range && write.offset < previous_end) [[unlikely]] {
                return std::unexpected(
                    make_error(WalErrorCode::InvalidRecord, "Overlapping file write ranges")
                );
            }
            if (final_size && range_end > *final_size) [[unlikely]] {
                return std::unexpected(make_error(
                    WalErrorCode::InvalidRecord,
                    "File write exceeds final truncated size"
                ));
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

std::expected<void, WalError> FileWriteBatch::normalize()
{
    auto normalized = normalize_writes(writes_);
    if (!normalized) [[unlikely]] {
        return std::unexpected(std::move(normalized.error()));
    }
    writes_ = std::move(*normalized);
    return {};
}

std::expected<std::filesystem::path, WalError> FileWriteBatch::resolve_target(
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
        return data_directory / "vindexes" /
               ("vindex_" + std::to_string(target.object_id) + ".lhnsw");
    case FileKind::CatalogStore:
        return data_directory / "catalog.lcat";
    default:
        [[unlikely]]
        {
            return std::unexpected(
                make_error(WalErrorCode::InvalidRecord, "Unknown WAL file target kind")
            );
        }
    }
}

std::expected<void, WalError> FileWriteBatch::apply(
    const std::filesystem::path & data_directory,
    filesystem::FileSystem & filesystem,
    bool sync,
    const FileWriteAppliedHook & applied_hook
) const
{
    auto normalized = normalize_writes(writes_);
    if (!normalized) [[unlikely]] {
        return std::unexpected(std::move(normalized.error()));
    }

    std::optional<FileTarget> current_target;
    std::optional<filesystem::FileHandle> current_file;
    auto sync_current = [&]() -> std::expected<void, WalError> {
        if (!sync || !current_file) {
            return {};
        }
        auto path = resolve_target(data_directory, *current_target);
        if (!path) [[unlikely]] {
            return std::unexpected(std::move(path.error()));
        }
        auto synced = current_file->sync_data();
        if (!synced) [[unlikely]] {
            return std::unexpected(std::move(synced.error()));
        }
        return {};
    };

    std::size_t applied_count {0};
    for (const auto & write : *normalized) {
        auto path = resolve_target(data_directory, write.target);
        if (!path) [[unlikely]] {
            return std::unexpected(std::move(path.error()));
        }
        if (current_target && *current_target != write.target) {
            auto synced = sync_current();
            if (!synced) [[unlikely]] {
                return std::unexpected(std::move(synced.error()));
            }
            current_file.reset();
        }
        current_target = write.target;
        auto exists = filesystem.exists(*path);
        if (!exists) [[unlikely]] {
            return std::unexpected(std::move(exists.error()));
        }
        if (write.mode == FileWriteMode::Delete) {
            current_file.reset();
            if (*exists) {
                auto removed = filesystem.remove(*path);
                if (!removed) [[unlikely]] {
                    return std::unexpected(std::move(removed.error()));
                }
            }
            ++applied_count;
            if (applied_hook && applied_hook(applied_count, write)) [[unlikely]] {
                return std::unexpected(make_error(
                    WalErrorCode::ApplyInterrupted,
                    "File write apply interrupted",
                    {
                        .operation = WalOperation::Apply,
                        .path = *path,
                    }
                ));
            }
            continue;
        }

        auto parent_created = filesystem.create_dir_all(path->parent_path());
        if (!parent_created) [[unlikely]] {
            return std::unexpected(std::move(parent_created.error()));
        }
        if (!current_file) {
            auto file = filesystem.open(
                *path,
                {
                    .access = filesystem::FileAccess::ReadWrite,
                    .create_mode = write.mode == FileWriteMode::Replace
                                       ? filesystem::FileCreateMode::CreateOrTruncate
                                       : filesystem::FileCreateMode::OpenOrCreate,
                }
            );
            if (!file) [[unlikely]] {
                return std::unexpected(std::move(file.error()));
            }
            current_file.emplace(std::move(*file));
        }
        if (write.mode == FileWriteMode::Truncate) {
            auto truncated = current_file->truncate(write.offset);
            if (!truncated) [[unlikely]] {
                return std::unexpected(std::move(truncated.error()));
            }
        } else {
            auto written = current_file->write_at(write.offset, write.after_image);
            if (!written) [[unlikely]] {
                return std::unexpected(std::move(written.error()));
            }
        }
        if (write.mode == FileWriteMode::Replace) {
            auto truncated = current_file->truncate(write.after_image.size());
            if (!truncated) [[unlikely]] {
                return std::unexpected(std::move(truncated.error()));
            }
        }
        ++applied_count;
        if (applied_hook && applied_hook(applied_count, write)) [[unlikely]] {
            return std::unexpected(make_error(
                WalErrorCode::ApplyInterrupted,
                "File write apply interrupted",
                {
                    .operation = WalOperation::Apply,
                    .path = *path,
                }
            ));
        }
    }
    return sync_current();
}

} // namespace litedb::core::wal

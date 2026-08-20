#include "core/wal/wal_manager.hpp"

#include <algorithm>
#include <charconv>
#include <iomanip>
#include <limits>
#include <optional>
#include <sstream>
#include <string_view>
#include <utility>
#include <vector>

namespace litedb::core::wal
{

namespace
{

// 从固定宽度的段文件名解析 generation，非法名称不参与发现。
[[nodiscard]]
std::optional<std::uint64_t> parse_generation(const std::filesystem::path & path)
{
    const auto name = path.filename().string();
    constexpr std::string_view suffix = ".wal";
    if (name.size() != 20 + suffix.size() || !name.ends_with(suffix)) {
        return std::nullopt;
    }
    std::uint64_t generation = 0;
    const auto result = std::from_chars(name.data(), name.data() + 20, generation);
    if (result.ec != std::errc {} || result.ptr != name.data() + 20 || generation == 0) {
        return std::nullopt;
    }
    return generation;
}

// 同步目录；平台不支持目录同步时保留现有可用行为。
[[nodiscard]]
std::expected<void, WalError>
sync_directory_if_supported(filesystem::FileSystem & filesystem, const std::filesystem::path & path)
{
    auto synced = filesystem.sync_directory(path);
    if (!synced && !synced.error().is(filesystem::FileSystemErrorCode::Unsupported)) [[unlikely]] {
        return std::unexpected(std::move(synced.error()));
    }
    return {};
}

} // namespace

WalManager::WalManager(
    std::filesystem::path directory,
    filesystem::FileSystem & filesystem,
    WalStore active,
    std::size_t retained_segments,
    WalDecodeLimits limits
) noexcept
    : directory_(std::move(directory))
    , filesystem_(&filesystem)
    , active_(std::move(active))
    , retained_segments_(retained_segments)
    , limits_(limits)
{}

std::filesystem::path
WalManager::segment_path(const std::filesystem::path & directory, std::uint64_t generation)
{
    std::ostringstream name;
    name << std::setw(20) << std::setfill('0') << generation << ".wal";
    return directory / name.str();
}

std::expected<WalManager, WalError> WalManager::open(
    std::filesystem::path directory,
    filesystem::FileSystem & filesystem,
    WalDecodeLimits limits
)
{
    auto created = filesystem.create_dir_all(directory);
    if (!created) [[unlikely]] {
        return std::unexpected(std::move(created.error()));
    }
    auto entries = filesystem.list_dir(directory);
    if (!entries) [[unlikely]] {
        return std::unexpected(std::move(entries.error()));
    }

    std::vector<std::pair<std::uint64_t, std::filesystem::path>> segments;
    bool removed_temporary = false;
    for (const auto & entry : *entries) {
        if (auto generation = parse_generation(entry)) {
            segments.emplace_back(*generation, directory / entry);
        } else if (entry.filename().string().ends_with(".wal.tmp")) {
            auto removed = filesystem.remove(directory / entry);
            if (!removed) [[unlikely]] {
                return std::unexpected(std::move(removed.error()));
            }
            removed_temporary = true;
        }
    }
    if (removed_temporary) {
        auto synced = sync_directory_if_supported(filesystem, directory);
        if (!synced) [[unlikely]] {
            return std::unexpected(std::move(synced.error()));
        }
    }
    std::sort(segments.begin(), segments.end(), [](const auto & left, const auto & right) {
        return left.first < right.first;
    });

    if (segments.empty()) {
        auto active = WalStore::create(
            segment_path(directory, 1),
            filesystem,
            WalFileHeader {.generation = 1}
        );
        if (!active) [[unlikely]] {
            return std::unexpected(std::move(active.error()));
        }
        auto synced = sync_directory_if_supported(filesystem, directory);
        if (!synced) [[unlikely]] {
            return std::unexpected(std::move(synced.error()));
        }
        return WalManager {std::move(directory), filesystem, std::move(*active), 1, limits};
    }

    const auto [generation, path] = segments.back();
    auto active = WalStore::open(path, filesystem);
    if (!active) [[unlikely]] {
        return std::unexpected(std::move(active.error()));
    }
    if (active->header().generation != generation) [[unlikely]] {
        return std::unexpected(make_error(
            WalErrorCode::InvalidFormat,
            "WAL generation does not match its file name",
            {
                .operation = WalOperation::Discover,
                .path = path,
                .generation = generation,
            }
        ));
    }
    return WalManager {
        std::move(directory),
        filesystem,
        std::move(*active),
        segments.size(),
        limits,
    };
}

std::expected<void, WalError> WalManager::validate_transaction(
    std::span<const FileWrite> writes
) const
{
    constexpr std::uint64_t FileWritePayloadHeaderSize = 24;
    if (limits_.max_record_size_bytes < WalCodec::RecordHeaderSize) [[unlikely]] {
        return std::unexpected(make_error(
            WalErrorCode::ResourceLimitExceeded,
            "WAL transaction markers exceed the configured recovery record limit",
            {
                .operation = WalOperation::Append,
                .path = active_.path(),
            }
        ));
    }
    if (writes.size() > std::numeric_limits<std::size_t>::max() - 2) [[unlikely]] {
        return std::unexpected(make_error(
            WalErrorCode::ResourceLimitExceeded,
            "WAL transaction record count overflows",
            {
                .operation = WalOperation::Append,
                .path = active_.path(),
            }
        ));
    }
    const auto additional_records = writes.size() + 2;
    if (additional_records > limits_.max_record_count ||
        record_count_ > limits_.max_record_count - additional_records) [[unlikely]] {
        return std::unexpected(make_error(
            WalErrorCode::ResourceLimitExceeded,
            "WAL transaction would exceed the configured record-count limit",
            {
                .operation = WalOperation::Append,
                .path = active_.path(),
            }
        ));
    }
    std::uint64_t additional_bytes = 2 * WalCodec::RecordHeaderSize;
    for (const auto & write : writes) {
        if (write.after_image.size() > std::numeric_limits<std::uint64_t>::max() -
                                           FileWritePayloadHeaderSize - WalCodec::RecordHeaderSize)
            [[unlikely]] {
            return std::unexpected(make_error(
                WalErrorCode::ResourceLimitExceeded,
                "WAL transaction record size overflows",
                {
                    .operation = WalOperation::Append,
                    .path = active_.path(),
                }
            ));
        }
        const auto record_size = static_cast<std::uint64_t>(WalCodec::RecordHeaderSize) +
                                 FileWritePayloadHeaderSize +
                                 static_cast<std::uint64_t>(write.after_image.size());
        if (record_size > limits_.max_record_size_bytes) [[unlikely]] {
            return std::unexpected(make_error(
                WalErrorCode::ResourceLimitExceeded,
                "WAL transaction record exceeds the configured recovery limit",
                {
                    .operation = WalOperation::Append,
                    .path = active_.path(),
                }
            ));
        }
        if (additional_bytes > std::numeric_limits<std::uint64_t>::max() - record_size)
            [[unlikely]] {
            return std::unexpected(make_error(
                WalErrorCode::ResourceLimitExceeded,
                "WAL transaction size overflows",
                {
                    .operation = WalOperation::Append,
                    .path = active_.path(),
                }
            ));
        }
        additional_bytes += record_size;
    }
    const auto current_size = active_.size_bytes();
    if (current_size > limits_.max_scan_size_bytes ||
        additional_bytes > limits_.max_scan_size_bytes - current_size) [[unlikely]] {
        return std::unexpected(make_error(
            WalErrorCode::ResourceLimitExceeded,
            "WAL transaction would exceed the configured recovery scan limit",
            {
                .operation = WalOperation::Append,
                .path = active_.path(),
            }
        ));
    }
    return {};
}

std::expected<transaction::Lsn, WalError> WalManager::append_begin(
    transaction::TransactionId transaction_id
)
{
    auto appended = active_.append_begin(transaction_id);
    if (appended) {
        ++record_count_;
    }
    return appended;
}

std::expected<transaction::Lsn, WalError>
WalManager::append_write(transaction::TransactionId transaction_id, const FileWrite & write)
{
    auto appended = active_.append_write(transaction_id, write);
    if (appended) {
        ++record_count_;
    }
    return appended;
}

std::expected<transaction::Lsn, WalError> WalManager::append_commit(
    transaction::TransactionId transaction_id
)
{
    auto appended = active_.append_commit(transaction_id);
    if (appended) {
        ++record_count_;
    }
    return appended;
}

std::expected<void, WalError> WalManager::flush_through(transaction::Lsn lsn)
{
    return active_.flush_through(lsn);
}

std::expected<void, WalError> WalManager::flush_all()
{
    return active_.flush_all();
}

std::expected<WalScanResult, WalError>
WalManager::scan(bool truncate_incomplete_tail, const WalDecodeLimits & limits)
{
    auto scanned = active_.scan(truncate_incomplete_tail, limits);
    if (scanned) {
        record_count_ = scanned->records.size();
    }
    return scanned;
}

std::expected<std::uint64_t, WalError> WalManager::rotate(
    transaction::TransactionId checkpoint_transaction_id,
    const WalRotationHook & hook
)
{
    if (active_.recovery_required()) [[unlikely]] {
        return std::unexpected(make_error(
            WalErrorCode::RecoveryRequired,
            "WAL rotation requires recovery",
            {
                .operation = WalOperation::Rotate,
                .path = active_.path(),
                .generation = active_.header().generation,
            }
        ));
    }
    if (active_.header().generation == std::numeric_limits<std::uint64_t>::max()) [[unlikely]] {
        return std::unexpected(
            make_error(WalErrorCode::InvalidRecord, "WAL generation space is exhausted")
        );
    }
    const auto next_generation = active_.header().generation + 1;
    const auto final_path = segment_path(directory_, next_generation);
    auto temporary_path = final_path;
    temporary_path += ".tmp";

    auto exists = filesystem_->exists(final_path);
    if (!exists) [[unlikely]] {
        return std::unexpected(std::move(exists.error()));
    }
    if (*exists) [[unlikely]] {
        return std::unexpected(make_error(
            WalErrorCode::InvalidFormat,
            "Next WAL generation already exists",
            {
                .operation = WalOperation::Rotate,
                .path = final_path,
                .generation = next_generation,
            }
        ));
    }
    exists = filesystem_->exists(temporary_path);
    if (!exists) [[unlikely]] {
        return std::unexpected(std::move(exists.error()));
    }
    if (*exists) {
        auto removed = filesystem_->remove(temporary_path);
        if (!removed) [[unlikely]] {
            return std::unexpected(std::move(removed.error()));
        }
    }

    {
        auto temporary = WalStore::create(
            temporary_path,
            *filesystem_,
            WalFileHeader {
                .generation = next_generation,
                .checkpoint_transaction_id = checkpoint_transaction_id,
            }
        );
        if (!temporary) [[unlikely]] {
            return std::unexpected(std::move(temporary.error()));
        }
        auto flushed = temporary->flush_all();
        if (!flushed) [[unlikely]] {
            return std::unexpected(std::move(flushed.error()));
        }
    }
    if (hook) {
        hook(WalRotationStage::AfterTemporarySync);
    }

    auto renamed = filesystem_->rename(temporary_path, final_path);
    if (!renamed) [[unlikely]] {
        return std::unexpected(std::move(renamed.error()));
    }
    if (hook) {
        hook(WalRotationStage::AfterPublish);
    }
    auto directory_synced = sync_directory_if_supported(*filesystem_, directory_);
    if (!directory_synced) [[unlikely]] {
        return std::unexpected(std::move(directory_synced.error()));
    }
    if (hook) {
        hook(WalRotationStage::AfterDirectorySync);
    }

    auto next = WalStore::open(final_path, *filesystem_);
    if (!next) [[unlikely]] {
        return std::unexpected(std::move(next.error()));
    }
    active_ = std::move(*next);
    ++retained_segments_;
    record_count_ = 0;
    if (hook) {
        hook(WalRotationStage::AfterSwitch);
    }

    auto entries = filesystem_->list_dir(directory_);
    if (!entries) [[unlikely]] {
        return std::unexpected(std::move(entries.error()));
    }
    bool removed_any = false;
    retained_segments_ = 0;
    for (const auto & entry : *entries) {
        const auto generation = parse_generation(entry);
        if (!generation) {
            continue;
        }
        if (*generation < next_generation) {
            auto removed = filesystem_->remove(directory_ / entry);
            if (!removed) [[unlikely]] {
                return std::unexpected(std::move(removed.error()));
            }
            removed_any = true;
        } else {
            ++retained_segments_;
        }
    }
    if (removed_any) {
        if (hook) {
            hook(WalRotationStage::AfterOldSegmentRemoval);
        }
        directory_synced = sync_directory_if_supported(*filesystem_, directory_);
        if (!directory_synced) [[unlikely]] {
            return std::unexpected(std::move(directory_synced.error()));
        }
    }
    return next_generation;
}

WalManagerMetrics WalManager::metrics() const noexcept
{
    return WalManagerMetrics {
        .generation = active_.header().generation,
        .checkpoint_transaction_id = active_.header().checkpoint_transaction_id,
        .size_bytes = active_.size_bytes(),
        .retained_segments = retained_segments_,
    };
}

const WalFileHeader & WalManager::header() const noexcept
{
    return active_.header();
}

bool WalManager::recovery_required() const noexcept
{
    return active_.recovery_required();
}

} // namespace litedb::core::wal

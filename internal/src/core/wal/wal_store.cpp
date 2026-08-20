#include "core/wal/wal_store.hpp"

#include <algorithm>
#include <array>
#include <limits>
#include <utility>

namespace litedb::core::wal
{

namespace
{

// 构造文件头或段结构不合法时使用的 WAL 格式错误。
[[nodiscard]]
WalError invalid_format(std::string message, const std::filesystem::path & path)
{
    return make_error(
        WalErrorCode::InvalidFormat,
        std::move(message),
        {
            .operation = WalOperation::Open,
            .path = path,
        }
    );
}

// 创建 WAL 所需的父目录；空父路径表示当前目录，无需操作。
[[nodiscard]]
std::expected<void, WalError>
create_parent_directory(filesystem::FileSystem & filesystem, const std::filesystem::path & path)
{
    if (path.has_parent_path() && !path.parent_path().empty()) {
        auto created = filesystem.create_dir_all(path.parent_path());
        if (!created) [[unlikely]] {
            return std::unexpected(std::move(created.error()));
        }
    }
    return {};
}

} // namespace

WalStore::WalStore(
    std::filesystem::path path,
    filesystem::FileHandle file,
    WalFileHeader header,
    std::uint64_t size_bytes
) noexcept
    : path_(std::move(path))
    , file_(std::move(file))
    , header_(header)
    , size_bytes_(size_bytes)
{}

std::expected<WalStore, WalError> WalStore::create(
    std::filesystem::path path,
    filesystem::FileSystem & filesystem,
    WalFileHeader header
)
{
    auto encoded_header = WalCodec::encode_file_header(header);
    if (!encoded_header) [[unlikely]] {
        return std::unexpected(std::move(encoded_header.error()));
    }
    auto parent_created = create_parent_directory(filesystem, path);
    if (!parent_created) [[unlikely]] {
        return std::unexpected(std::move(parent_created.error()));
    }
    auto opened = filesystem.open(
        path,
        {
            .access = filesystem::FileAccess::ReadWrite,
            .create_mode = filesystem::FileCreateMode::CreateNew,
        }
    );
    if (!opened) [[unlikely]] {
        return std::unexpected(std::move(opened.error()));
    }

    auto written = opened->write_at(0, *encoded_header);
    if (!written) [[unlikely]] {
        auto error = std::move(written.error());
        static_cast<void>(opened->close());
        static_cast<void>(filesystem.remove(path));
        return std::unexpected(std::move(error));
    }
    auto synced = opened->sync_all();
    if (!synced) [[unlikely]] {
        auto error = std::move(synced.error());
        static_cast<void>(opened->close());
        static_cast<void>(filesystem.remove(path));
        return std::unexpected(std::move(error));
    }
    return WalStore {std::move(path), std::move(*opened), header, WalCodec::FileHeaderSize};
}

std::expected<WalStore, WalError>
WalStore::open(std::filesystem::path path, filesystem::FileSystem & filesystem)
{
    auto opened = filesystem.open(
        path,
        {
            .access = filesystem::FileAccess::ReadWrite,
            .create_mode = filesystem::FileCreateMode::OpenExisting,
        }
    );
    if (!opened) [[unlikely]] {
        return std::unexpected(std::move(opened.error()));
    }
    auto size = opened->size();
    if (!size) [[unlikely]] {
        return std::unexpected(std::move(size.error()));
    }
    if (*size < WalCodec::FileHeaderSize) [[unlikely]] {
        return std::unexpected(invalid_format("WAL file header is truncated", path));
    }
    WalCodec::FileHeader encoded_header {};
    auto read = opened->read_at(0, encoded_header);
    if (!read) [[unlikely]] {
        return std::unexpected(std::move(read.error()));
    }
    if (*read != encoded_header.size()) [[unlikely]] {
        return std::unexpected(invalid_format("WAL file header is truncated", path));
    }
    auto decoded_header = WalCodec::decode_file_header(encoded_header);
    if (!decoded_header) [[unlikely]] {
        return std::unexpected(std::move(decoded_header.error()));
    }
    return WalStore {std::move(path), std::move(*opened), *decoded_header, *size};
}

std::expected<transaction::Lsn, WalError> WalStore::append(
    WalRecordType type,
    transaction::TransactionId transaction_id,
    std::span<const std::byte> payload
)
{
    if (recovery_required_) [[unlikely]] {
        return std::unexpected(recovery_error(WalOperation::Append));
    }
    auto size = file_.size();
    if (!size) [[unlikely]] {
        return std::unexpected(std::move(size.error()));
    }
    if (*size < WalCodec::FileHeaderSize) [[unlikely]] {
        return std::unexpected(invalid_format("WAL file header is truncated", path_));
    }
    auto encoded = WalCodec::encode_record(type, *size, transaction_id, payload);
    if (!encoded) [[unlikely]] {
        return std::unexpected(std::move(encoded.error()));
    }
    if (*size > std::numeric_limits<std::uint64_t>::max() - encoded->size()) [[unlikely]] {
        return std::unexpected(make_error(
            WalErrorCode::InvalidRecord,
            "WAL file size overflows",
            {
                .operation = WalOperation::Append,
                .path = path_,
                .transaction_id = transaction_id,
                .lsn = *size,
            }
        ));
    }
    auto appended = file_.append(*encoded);
    if (!appended) [[unlikely]] {
        auto append_error = std::move(appended.error());
        auto truncated = file_.truncate(*size);
        if (!truncated) [[unlikely]] {
            recovery_required_ = true;
            return std::unexpected(std::move(truncated.error()));
        }
        auto synchronized = file_.sync_data();
        if (!synchronized) [[unlikely]] {
            recovery_required_ = true;
            return std::unexpected(std::move(synchronized.error()));
        }
        size_bytes_ = *size;
        return std::unexpected(std::move(append_error));
    }
    size_bytes_ = *size + encoded->size();
    return *size;
}

std::expected<transaction::Lsn, WalError> WalStore::append_begin(
    transaction::TransactionId transaction_id
)
{
    return append(WalRecordType::Begin, transaction_id, {});
}

std::expected<transaction::Lsn, WalError>
WalStore::append_write(transaction::TransactionId transaction_id, const FileWrite & write)
{
    auto payload = WalCodec::encode_file_write(write);
    if (!payload) [[unlikely]] {
        return std::unexpected(std::move(payload.error()));
    }
    return append(WalRecordType::FileWrite, transaction_id, *payload);
}

std::expected<transaction::Lsn, WalError> WalStore::append_commit(
    transaction::TransactionId transaction_id
)
{
    return append(WalRecordType::Commit, transaction_id, {});
}

std::expected<void, WalError> WalStore::flush_through(transaction::Lsn lsn)
{
    if (recovery_required_) [[unlikely]] {
        return std::unexpected(recovery_error(WalOperation::Flush, lsn));
    }
    auto size = file_.size();
    if (!size) [[unlikely]] {
        return std::unexpected(std::move(size.error()));
    }
    if (lsn < WalCodec::FileHeaderSize || lsn >= *size) [[unlikely]] {
        return std::unexpected(make_error(
            WalErrorCode::InvalidRecord,
            "Cannot flush through an unknown LSN",
            {
                .operation = WalOperation::Flush,
                .path = path_,
                .lsn = lsn,
            }
        ));
    }
    auto synced = file_.sync_data();
    if (!synced) [[unlikely]] {
        recovery_required_ = true;
        return std::unexpected(std::move(synced.error()));
    }
    flushed_lsn_ = lsn;
    return {};
}

std::expected<void, WalError> WalStore::flush_all()
{
    if (recovery_required_) [[unlikely]] {
        return std::unexpected(recovery_error(WalOperation::Flush));
    }
    auto synced = file_.sync_all();
    if (!synced) [[unlikely]] {
        recovery_required_ = true;
        return std::unexpected(std::move(synced.error()));
    }
    return {};
}

std::expected<WalScanResult, WalError>
WalStore::scan(bool truncate_incomplete_tail, const WalDecodeLimits & limits)
{
    if (recovery_required_ && truncate_incomplete_tail) [[unlikely]] {
        return std::unexpected(recovery_error(WalOperation::Scan));
    }
    auto size = file_.size();
    if (!size) [[unlikely]] {
        return std::unexpected(std::move(size.error()));
    }
    if (*size < WalCodec::FileHeaderSize) [[unlikely]] {
        return std::unexpected(invalid_format("WAL file header is truncated", path_));
    }
    if (*size > limits.max_scan_size_bytes) [[unlikely]] {
        return std::unexpected(make_error(
            WalErrorCode::ResourceLimitExceeded,
            "WAL scan size exceeds the configured resource limit",
            {
                .operation = WalOperation::Scan,
                .path = path_,
            }
        ));
    }

    WalScanResult result {.valid_size = WalCodec::FileHeaderSize};
    std::uint64_t offset = WalCodec::FileHeaderSize;
    while (offset < *size) {
        const auto remaining = *size - offset;
        if (remaining < WalCodec::RecordHeaderSize) [[unlikely]] {
            result.truncated_tail = true;
            break;
        }
        std::array<std::byte, WalCodec::RecordHeaderSize> header {};
        auto header_read = file_.read_at(offset, header);
        if (!header_read) [[unlikely]] {
            return std::unexpected(std::move(header_read.error()));
        }
        if (*header_read != header.size()) [[unlikely]] {
            result.truncated_tail = true;
            break;
        }
        auto record_size = WalCodec::decode_record_size(header, offset);
        if (!record_size) [[unlikely]] {
            return std::unexpected(std::move(record_size.error()));
        }
        if (*record_size > remaining) [[unlikely]] {
            result.truncated_tail = true;
            break;
        }
        if (*record_size > std::numeric_limits<std::size_t>::max()) [[unlikely]] {
            return std::unexpected(make_error(
                WalErrorCode::CorruptedRecord,
                "WAL record is too large",
                {
                    .operation = WalOperation::Scan,
                    .path = path_,
                    .lsn = offset,
                }
            ));
        }
        if (*record_size > limits.max_record_size_bytes) [[unlikely]] {
            return std::unexpected(make_error(
                WalErrorCode::ResourceLimitExceeded,
                "WAL record size exceeds the configured resource limit",
                {
                    .operation = WalOperation::Scan,
                    .path = path_,
                    .lsn = offset,
                }
            ));
        }
        if (result.records.size() >= limits.max_record_count) [[unlikely]] {
            return std::unexpected(make_error(
                WalErrorCode::ResourceLimitExceeded,
                "WAL record count exceeds the configured resource limit",
                {
                    .operation = WalOperation::Scan,
                    .path = path_,
                    .lsn = offset,
                }
            ));
        }
        std::vector<std::byte> bytes(static_cast<std::size_t>(*record_size));
        auto read = file_.read_at(offset, bytes);
        if (!read) [[unlikely]] {
            return std::unexpected(std::move(read.error()));
        }
        if (*read != bytes.size()) [[unlikely]] {
            result.truncated_tail = true;
            break;
        }
        auto decoded = WalCodec::decode_record(std::move(bytes), offset);
        if (!decoded) [[unlikely]] {
            return std::unexpected(std::move(decoded.error()));
        }
        result.maximum_transaction_id =
            std::max(result.maximum_transaction_id, decoded->transaction_id);
        result.records.push_back(std::move(*decoded));
        offset += *record_size;
        result.valid_size = offset;
    }
    if (result.truncated_tail && truncate_incomplete_tail) {
        auto truncated = truncate_tail(result.valid_size);
        if (!truncated) [[unlikely]] {
            return std::unexpected(std::move(truncated.error()));
        }
    } else {
        size_bytes_ = *size;
    }
    return result;
}

std::expected<void, WalError> WalStore::truncate_tail(std::uint64_t valid_size)
{
    if (recovery_required_) [[unlikely]] {
        return std::unexpected(recovery_error(WalOperation::Truncate, valid_size));
    }
    auto size = file_.size();
    if (!size) [[unlikely]] {
        return std::unexpected(std::move(size.error()));
    }
    if (valid_size < WalCodec::FileHeaderSize || valid_size > *size) [[unlikely]] {
        return std::unexpected(make_error(
            WalErrorCode::InvalidRecord,
            "Invalid WAL truncation size",
            {
                .operation = WalOperation::Truncate,
                .path = path_,
                .lsn = valid_size,
            }
        ));
    }
    auto truncated = file_.truncate(valid_size);
    if (!truncated) [[unlikely]] {
        recovery_required_ = true;
        return std::unexpected(std::move(truncated.error()));
    }
    auto synced = file_.sync_data();
    if (!synced) [[unlikely]] {
        recovery_required_ = true;
        return std::unexpected(std::move(synced.error()));
    }
    size_bytes_ = valid_size;
    return {};
}

const std::filesystem::path & WalStore::path() const noexcept
{
    return path_;
}

std::optional<transaction::Lsn> WalStore::flushed_lsn() const noexcept
{
    return flushed_lsn_;
}

std::uint64_t WalStore::size_bytes() const noexcept
{
    return size_bytes_;
}

const WalFileHeader & WalStore::header() const noexcept
{
    return header_;
}

bool WalStore::recovery_required() const noexcept
{
    return recovery_required_;
}

WalError WalStore::recovery_error(WalOperation operation, std::optional<transaction::Lsn> lsn) const
{
    return make_error(
        WalErrorCode::RecoveryRequired,
        "WAL recovery is required before mutation",
        {
            .operation = operation,
            .path = path_,
            .lsn = lsn,
            .generation = header_.generation,
        }
    );
}

} // namespace litedb::core::wal

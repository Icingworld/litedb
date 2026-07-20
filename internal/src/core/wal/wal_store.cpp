#include "core/wal/wal_store.hpp"

#include <algorithm>
#include <array>
#include <limits>
#include <utility>
#include <vector>

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

/**
 * @brief 读取数字
 * @param source 源数据
 * @return 数字
 */
template <typename T>
T read_number(const std::byte * source) noexcept
{
    T value {};
    for (std::size_t index = 0; index < sizeof(T); ++index) {
        value |= static_cast<T>(std::to_integer<std::uint8_t>(source[index])) << (index * 8U);
    }
    return value;
}

} // namespace

WalStore::WalStore(
    std::filesystem::path path,
    filesystem::FileHandle file,
    std::uint64_t size_bytes
) noexcept
    : path_(std::move(path))
    , file_(std::move(file))
    , size_bytes_(size_bytes)
{
}

std::expected<WalStore, WalError> WalStore::open(
    std::filesystem::path path,
    filesystem::FileSystem & filesystem
)
{
    if (auto created = filesystem.create_dir_all(path.parent_path()); !created) {
        return std::unexpected(fs_error(std::move(created.error())));
    }

    auto opened = filesystem.open(
        path,
        {
            filesystem::backend::FileAccess::ReadWrite,
            filesystem::backend::FileCreateMode::OpenOrCreate,
        }
    );
    if (!opened) {
        return std::unexpected(fs_error(std::move(opened.error())));
    }

    auto size = opened->size();
    if (!size) {
        return std::unexpected(fs_error(std::move(size.error())));
    }

    if (*size == 0) {
        const auto header = WalCodec::encode_file_header();
        auto written = opened->write_at(0, header);
        if (!written) {
            return std::unexpected(fs_error(std::move(written.error())));
        }
        auto synced = opened->sync_all();
        if (!synced) {
            return std::unexpected(fs_error(std::move(synced.error())));
        }
    } else if (*size < WalCodec::FileHeaderSize) {
        return std::unexpected(make_error(WalErrorCode::InvalidFormat, "WAL file header is truncated"));
    } else {
        WalCodec::FileHeader header {};
        auto read = opened->read_at(0, header);
        if (!read) {
            return std::unexpected(fs_error(std::move(read.error())));
        }
        if (*read != header.size()) {
            return std::unexpected(make_error(WalErrorCode::InvalidFormat, "WAL file header is truncated"));
        }
        auto decoded = WalCodec::decode_file_header(header);
        if (!decoded) {
            return std::unexpected(std::move(decoded.error()));
        }
    }

    auto final_size = opened->size();
    if (!final_size) {
        return std::unexpected(fs_error(std::move(final_size.error())));
    }
    return WalStore {std::move(path), std::move(*opened), *final_size};
}

std::expected<transaction::Lsn, WalError> WalStore::append(
    WalRecordType type,
    transaction::TransactionId transaction_id,
    std::span<const std::byte> payload
)
{
    auto size = file_.size();
    if (!size) {
        return std::unexpected(fs_error(std::move(size.error())));
    }

    auto encoded = WalCodec::encode_record(type, *size, transaction_id, payload);
    if (!encoded) {
        return std::unexpected(std::move(encoded.error()));
    }

    auto appended = file_.append(*encoded);
    if (!appended) {
        return std::unexpected(fs_error(std::move(appended.error())));
    }
    size_bytes_ = *size + encoded->size();
    return *size;
}

std::expected<transaction::Lsn, WalError> WalStore::append_begin(transaction::TransactionId transaction_id)
{
    return append(WalRecordType::Begin, transaction_id, {});
}

std::expected<transaction::Lsn, WalError> WalStore::append_write(
    transaction::TransactionId transaction_id,
    const FileWrite & write
)
{
    auto payload = WalCodec::encode_file_write(write);
    return append(WalRecordType::FileWrite, transaction_id, payload);
}

std::expected<transaction::Lsn, WalError> WalStore::append_commit(transaction::TransactionId transaction_id)
{
    return append(WalRecordType::Commit, transaction_id, {});
}

std::expected<void, WalError> WalStore::flush_through(transaction::Lsn lsn)
{
    auto size = file_.size();
    if (!size) {
        return std::unexpected(fs_error(std::move(size.error())));
    }
    if (lsn < WalCodec::FileHeaderSize || lsn >= *size) {
        return std::unexpected(make_error(WalErrorCode::InvalidRecord, "Cannot flush through an unknown LSN"));
    }

    auto synced = file_.sync_data();
    if (!synced) {
        return std::unexpected(fs_error(std::move(synced.error())));
    }
    flushed_lsn_ = lsn;
    return {};
}

std::expected<WalScanResult, WalError> WalStore::scan(bool truncate_incomplete_tail)
{
    auto size = file_.size();
    if (!size) {
        return std::unexpected(fs_error(std::move(size.error())));
    }
    if (*size < WalCodec::FileHeaderSize) {
        return std::unexpected(make_error(WalErrorCode::InvalidFormat, "WAL file header is truncated"));
    }

    WalScanResult result {.valid_size = WalCodec::FileHeaderSize};
    std::uint64_t offset = WalCodec::FileHeaderSize;
    while (offset < *size) {
        const auto remaining = *size - offset;
        if (remaining < WalCodec::RecordHeaderSize) {
            result.truncated_tail = true;
            break;
        }

        std::array<std::byte, WalCodec::RecordHeaderSize> header {};
        auto header_read = file_.read_at(offset, header);
        if (!header_read) {
            return std::unexpected(fs_error(std::move(header_read.error())));
        }
        if (*header_read != header.size()) {
            result.truncated_tail = true;
            break;
        }

        const auto record_size = read_number<std::uint64_t>(header.data() + 8);
        if (record_size < WalCodec::RecordHeaderSize) {
            return std::unexpected(make_error(WalErrorCode::CorruptedRecord, "WAL record size is invalid"));
        }
        if (record_size > remaining) {
            result.truncated_tail = true;
            break;
        }
        if (record_size > std::numeric_limits<std::size_t>::max()) {
            return std::unexpected(make_error(WalErrorCode::CorruptedRecord, "WAL record is too large"));
        }

        std::vector<std::byte> bytes(static_cast<std::size_t>(record_size));
        auto read = file_.read_at(offset, bytes);
        if (!read) {
            return std::unexpected(fs_error(std::move(read.error())));
        }
        if (*read != bytes.size()) {
            result.truncated_tail = true;
            break;
        }

        auto decoded = WalCodec::decode_record(bytes, offset);
        if (!decoded) {
            return std::unexpected(std::move(decoded.error()));
        }

        result.maximum_transaction_id = std::max(result.maximum_transaction_id, decoded->transaction_id);
        result.records.push_back(std::move(*decoded));
        offset += record_size;
        result.valid_size = offset;
    }

    if (result.truncated_tail && truncate_incomplete_tail) {
        auto truncated = truncate_tail(result.valid_size);
        if (!truncated) {
            return std::unexpected(std::move(truncated.error()));
        }
    }
    return result;
}

std::expected<void, WalError> WalStore::truncate_tail(std::uint64_t valid_size)
{
    if (valid_size < WalCodec::FileHeaderSize) {
        return std::unexpected(make_error(WalErrorCode::InvalidRecord, "Cannot truncate WAL before its header"));
    }

    auto truncated = file_.truncate(valid_size);
    if (!truncated) {
        return std::unexpected(fs_error(std::move(truncated.error())));
    }
    size_bytes_ = valid_size;

    auto synced = file_.sync_data();
    if (!synced) {
        return std::unexpected(fs_error(std::move(synced.error())));
    }
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

} // namespace litedb::core::wal

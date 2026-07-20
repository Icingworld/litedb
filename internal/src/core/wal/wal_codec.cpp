#include "core/wal/wal_codec.hpp"

#include <algorithm>
#include <cstring>
#include <limits>
#include <string>

namespace litedb::core::wal
{

namespace
{

constexpr std::uint32_t FileMagic = 0x4c57444cU;    // LDWL
constexpr std::uint32_t RecordMagic = 0x3152574cU;  // LWR1
constexpr std::uint16_t Version = 2;

/**
 * @brief 写入数字
 * @param target 目标数据
 * @param value 数字
 */
template <typename T>
void write_number(std::byte * target, T value) noexcept
{
    for (std::size_t index = 0; index < sizeof(T); ++index) {
        target[index] = static_cast<std::byte>((value >> (index * 8U)) & static_cast<T>(0xffU));
    }
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

/**
 * @brief 计算 CRC32
 * @param bytes 字节数据
 * @return 校验和
 */
[[nodiscard]]
std::uint32_t crc32(std::span<const std::byte> bytes) noexcept
{
    std::uint32_t crc = 0xffffffffU;
    for (const auto byte : bytes) {
        crc ^= std::to_integer<std::uint8_t>(byte);
        for (std::uint32_t bit = 0; bit < 8; ++bit) {
            const auto mask = static_cast<std::uint32_t>(-(static_cast<std::int32_t>(crc & 1U)));
            crc = (crc >> 1U) ^ (0xedb88320U & mask);
        }
    }
    return ~crc;
}

/**
 * @brief 判断记录类型是否有效
 * @param value 类型值
 * @return 是否有效
 */
[[nodiscard]]
bool valid_type(std::uint8_t value) noexcept
{
    return value >= static_cast<std::uint8_t>(WalRecordType::Begin) &&
           value <= static_cast<std::uint8_t>(WalRecordType::Commit);
}

} // namespace

WalCodec::FileHeader WalCodec::encode_file_header(const WalFileHeader & value) noexcept
{
    FileHeader header {};
    write_number(header.data(), FileMagic);
    write_number(header.data() + 4, Version);
    write_number(header.data() + 6, static_cast<std::uint16_t>(FileHeaderSize));
    write_number(header.data() + 8, value.generation);
    write_number(header.data() + 16, value.checkpoint_transaction_id);
    write_number(header.data() + 24, static_cast<std::uint32_t>(0));
    write_number(header.data() + 28, static_cast<std::uint32_t>(0));
    write_number(header.data() + 24, crc32(header));
    return header;
}

std::expected<WalFileHeader, WalError> WalCodec::decode_file_header(std::span<const std::byte> bytes)
{
    if (bytes.size() != FileHeaderSize || read_number<std::uint32_t>(bytes.data()) != FileMagic) {
        return std::unexpected(make_error(WalErrorCode::InvalidFormat, "Invalid WAL file header"));
    }
    if (read_number<std::uint16_t>(bytes.data() + 4) != Version) {
        return std::unexpected(make_error(WalErrorCode::UnsupportedVersion, "Unsupported WAL version"));
    }
    if (read_number<std::uint16_t>(bytes.data() + 6) != FileHeaderSize ||
        read_number<std::uint64_t>(bytes.data() + 8) == 0 ||
        read_number<std::uint32_t>(bytes.data() + 28) != 0) {
        return std::unexpected(make_error(WalErrorCode::InvalidFormat, "Invalid WAL file header fields"));
    }
    auto checked = FileHeader {};
    std::copy(bytes.begin(), bytes.end(), checked.begin());
    const auto stored_checksum = read_number<std::uint32_t>(checked.data() + 24);
    write_number(checked.data() + 24, static_cast<std::uint32_t>(0));
    if (stored_checksum != crc32(checked)) {
        return std::unexpected(make_error(WalErrorCode::InvalidFormat, "WAL file header checksum mismatch"));
    }
    return WalFileHeader {
        .generation = read_number<std::uint64_t>(bytes.data() + 8),
        .checkpoint_transaction_id = read_number<transaction::TransactionId>(bytes.data() + 16),
    };
}

std::expected<std::vector<std::byte>, WalError> WalCodec::encode_record(
    WalRecordType type,
    transaction::Lsn lsn,
    transaction::TransactionId transaction_id,
    std::span<const std::byte> payload
)
{
    if (!valid_type(static_cast<std::uint8_t>(type)) ||
        transaction_id == transaction::InvalidTransactionId ||
        payload.size() > std::numeric_limits<std::uint32_t>::max()) {
        return std::unexpected(make_error(WalErrorCode::InvalidRecord, "Invalid WAL record"));
    }

    const auto total_size = RecordHeaderSize + payload.size();
    std::vector<std::byte> encoded(total_size);
    write_number(encoded.data(), RecordMagic);
    write_number(encoded.data() + 4, Version);
    write_number(encoded.data() + 6, static_cast<std::uint8_t>(type));
    write_number(encoded.data() + 7, static_cast<std::uint8_t>(0));
    write_number(encoded.data() + 8, static_cast<std::uint64_t>(total_size));
    write_number(encoded.data() + 16, lsn);
    write_number(encoded.data() + 24, transaction_id);
    write_number(encoded.data() + 32, static_cast<std::uint64_t>(payload.size()));
    write_number(encoded.data() + 40, static_cast<std::uint32_t>(0));
    write_number(encoded.data() + 44, static_cast<std::uint32_t>(0));
    std::copy(payload.begin(), payload.end(), encoded.begin() + RecordHeaderSize);
    write_number(encoded.data() + 40, crc32(encoded));
    return encoded;
}

std::expected<WalRecord, WalError> WalCodec::decode_record(
    std::span<const std::byte> bytes,
    transaction::Lsn expected_lsn
)
{
    if (bytes.size() < RecordHeaderSize || read_number<std::uint32_t>(bytes.data()) != RecordMagic) {
        return std::unexpected(make_error(WalErrorCode::CorruptedRecord, "Invalid WAL record header"));
    }
    if (read_number<std::uint16_t>(bytes.data() + 4) != Version) {
        return std::unexpected(make_error(WalErrorCode::UnsupportedVersion, "Unsupported WAL record version"));
    }

    const auto type_value = read_number<std::uint8_t>(bytes.data() + 6);
    const auto total_size = read_number<std::uint64_t>(bytes.data() + 8);
    const auto payload_size = read_number<std::uint64_t>(bytes.data() + 32);
    if (!valid_type(type_value) ||
        read_number<std::uint8_t>(bytes.data() + 7) != 0 ||
        read_number<std::uint32_t>(bytes.data() + 44) != 0 ||
        total_size != bytes.size() ||
        payload_size != bytes.size() - RecordHeaderSize ||
        read_number<transaction::Lsn>(bytes.data() + 16) != expected_lsn) {
        return std::unexpected(make_error(WalErrorCode::CorruptedRecord, "Invalid WAL record fields"));
    }

    auto checked = std::vector<std::byte>(bytes.begin(), bytes.end());
    const auto stored_checksum = read_number<std::uint32_t>(checked.data() + 40);
    write_number(checked.data() + 40, static_cast<std::uint32_t>(0));
    if (stored_checksum != crc32(checked)) {
        return std::unexpected(make_error(WalErrorCode::CorruptedRecord, "WAL record checksum mismatch"));
    }

    const auto transaction_id = read_number<transaction::TransactionId>(bytes.data() + 24);
    if (transaction_id == transaction::InvalidTransactionId) {
        return std::unexpected(make_error(WalErrorCode::CorruptedRecord, "WAL record has invalid transaction ID"));
    }

    return WalRecord {
        .type = static_cast<WalRecordType>(type_value),
        .lsn = expected_lsn,
        .transaction_id = transaction_id,
        .payload = std::vector<std::byte>(bytes.begin() + RecordHeaderSize, bytes.end()),
    };
}

std::vector<std::byte> WalCodec::encode_file_write(const FileWrite & write)
{
    std::vector<std::byte> payload(24 + write.after_image.size());
    write_number(payload.data(), static_cast<std::uint8_t>(write.target.kind));
    write_number(payload.data() + 1, static_cast<std::uint8_t>(write.mode));
    write_number(payload.data() + 8, write.target.object_id);
    write_number(payload.data() + 16, write.offset);
    std::copy(write.after_image.begin(), write.after_image.end(), payload.begin() + 24);
    return payload;
}

std::expected<FileWrite, WalError> WalCodec::decode_file_write(std::span<const std::byte> payload)
{
    if (payload.size() < 24 ||
        std::any_of(payload.begin() + 2, payload.begin() + 8, [](std::byte value) {
            return value != std::byte {0};
        })) {
        return std::unexpected(make_error(WalErrorCode::CorruptedRecord, "Invalid WAL file-write payload"));
    }

    const auto kind_value = read_number<std::uint8_t>(payload.data());
    if (kind_value < static_cast<std::uint8_t>(FileKind::CollectionStore) ||
        kind_value > static_cast<std::uint8_t>(FileKind::MetaStore)) {
        return std::unexpected(make_error(WalErrorCode::CorruptedRecord, "Unknown WAL file target kind"));
    }
    const auto mode_value = read_number<std::uint8_t>(payload.data() + 1);
    if (mode_value > static_cast<std::uint8_t>(FileWriteMode::Delete)) {
        return std::unexpected(make_error(WalErrorCode::CorruptedRecord, "Unknown WAL file-write mode"));
    }
    const auto mode = static_cast<FileWriteMode>(mode_value);
    const auto offset = read_number<std::uint64_t>(payload.data() + 16);
    const auto object_id = read_number<std::uint64_t>(payload.data() + 8);
    if ((mode == FileWriteMode::Replace && offset != 0) ||
        (mode == FileWriteMode::Delete && (offset != 0 || payload.size() != 24)) ||
        (kind_value == static_cast<std::uint8_t>(FileKind::MetaStore) && object_id != 0) ||
        (kind_value != static_cast<std::uint8_t>(FileKind::MetaStore) && object_id == 0)) {
        return std::unexpected(make_error(WalErrorCode::CorruptedRecord, "Invalid WAL file operation"));
    }

    return FileWrite {
        .target = FileTarget {
            .kind = static_cast<FileKind>(kind_value),
            .object_id = object_id,
        },
        .offset = offset,
        .after_image = std::vector<std::byte>(payload.begin() + 24, payload.end()),
        .mode = mode,
    };
}

} // namespace litedb::core::wal

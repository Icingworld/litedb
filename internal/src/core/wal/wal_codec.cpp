#include "core/wal/wal_codec.hpp"

#include <algorithm>
#include <limits>
#include <utility>

#include "core/io/binary_io.hpp"
#include "core/io/buffer_byte_reader.hpp"
#include "core/io/buffer_byte_writer.hpp"
#include "core/io/checksum.hpp"

namespace litedb::core::wal
{

namespace
{

constexpr std::uint32_t FileMagic = 0x4c57444cU;
constexpr std::uint32_t RecordMagic = 0x3152574cU;
constexpr std::uint16_t Version = 2;

// 判断记录类型是否属于 WAL v2 支持的连续枚举范围。
[[nodiscard]]
bool valid_type(std::uint8_t value) noexcept
{
    return value >= static_cast<std::uint8_t>(WalRecordType::Begin) &&
           value <= static_cast<std::uint8_t>(WalRecordType::Commit);
}

// 构造带解码上下文的记录损坏错误。
[[nodiscard]]
WalError invalid_record(std::string message, transaction::Lsn lsn = transaction::InvalidLsn)
{
    return make_error(
        WalErrorCode::CorruptedRecord,
        std::move(message),
        {
            .operation = WalOperation::Decode,
            .lsn = lsn,
        }
    );
}

// 使用小端序写入固定大小的 WAL 文件头。
[[nodiscard]]
std::expected<std::vector<std::byte>, WalError>
encode_file_header_bytes(const WalFileHeader & header, std::uint32_t checksum)
{
    io::BufferByteWriter bytes(WalCodec::FileHeaderSize);
    io::LittleEndianBinaryWriter writer(bytes);
    if (auto result = writer.write_u32(FileMagic); !result) [[unlikely]] {
        return std::unexpected(std::move(result.error()));
    }
    if (auto result = writer.write_u16(Version); !result) [[unlikely]] {
        return std::unexpected(std::move(result.error()));
    }
    if (auto result = writer.write_u16(static_cast<std::uint16_t>(WalCodec::FileHeaderSize));
        !result) [[unlikely]] {
        return std::unexpected(std::move(result.error()));
    }
    if (auto result = writer.write_u64(header.generation); !result) [[unlikely]] {
        return std::unexpected(std::move(result.error()));
    }
    if (auto result = writer.write_u64(header.checkpoint_transaction_id); !result) [[unlikely]] {
        return std::unexpected(std::move(result.error()));
    }
    if (auto result = writer.write_u32(checksum); !result) [[unlikely]] {
        return std::unexpected(std::move(result.error()));
    }
    if (auto result = writer.write_u32(0); !result) [[unlikely]] {
        return std::unexpected(std::move(result.error()));
    }
    return bytes.take_bytes();
}

// 使用小端序写入记录头和负载，checksum 字段由调用方提供。
[[nodiscard]]
std::expected<std::vector<std::byte>, WalError> encode_record_bytes(
    WalRecordType type,
    transaction::Lsn lsn,
    transaction::TransactionId transaction_id,
    std::span<const std::byte> payload,
    std::uint32_t checksum
)
{
    const auto total_size = WalCodec::RecordHeaderSize + payload.size();
    io::BufferByteWriter bytes(total_size);
    io::LittleEndianBinaryWriter writer(bytes);
    if (auto result = writer.write_u32(RecordMagic); !result) [[unlikely]] {
        return std::unexpected(std::move(result.error()));
    }
    if (auto result = writer.write_u16(Version); !result) [[unlikely]] {
        return std::unexpected(std::move(result.error()));
    }
    if (auto result = writer.write_u8(static_cast<std::uint8_t>(type)); !result) [[unlikely]] {
        return std::unexpected(std::move(result.error()));
    }
    if (auto result = writer.write_u8(0); !result) [[unlikely]] {
        return std::unexpected(std::move(result.error()));
    }
    if (auto result = writer.write_u64(total_size); !result) [[unlikely]] {
        return std::unexpected(std::move(result.error()));
    }
    if (auto result = writer.write_u64(lsn); !result) [[unlikely]] {
        return std::unexpected(std::move(result.error()));
    }
    if (auto result = writer.write_u64(transaction_id); !result) [[unlikely]] {
        return std::unexpected(std::move(result.error()));
    }
    if (auto result = writer.write_u64(payload.size()); !result) [[unlikely]] {
        return std::unexpected(std::move(result.error()));
    }
    if (auto result = writer.write_u32(checksum); !result) [[unlikely]] {
        return std::unexpected(std::move(result.error()));
    }
    if (auto result = writer.write_u32(0); !result) [[unlikely]] {
        return std::unexpected(std::move(result.error()));
    }
    if (!payload.empty()) {
        if (auto result = bytes.write_bytes(payload); !result) [[unlikely]] {
            return std::unexpected(std::move(result.error()));
        }
    }
    return bytes.take_bytes();
}

// 从固定偏移读取小端序 checksum 字段。
[[nodiscard]]
std::expected<std::uint32_t, WalError> read_checksum(std::span<const std::byte> bytes)
{
    io::BufferByteReader buffer(bytes);
    io::LittleEndianBinaryReader reader(
        buffer,
        {.max_total_bytes = bytes.size(), .max_string_bytes = 0}
    );
    auto checksum = reader.read_u32();
    if (!checksum) [[unlikely]] {
        return std::unexpected(std::move(checksum.error()));
    }
    return *checksum;
}

// 将 checksum 字段置零后重新计算 CRC32 并比较。
[[nodiscard]]
bool checksum_matches(
    std::span<const std::byte> bytes,
    std::size_t checksum_offset,
    std::uint32_t expected
)
{
    std::vector<std::byte> checked(bytes.begin(), bytes.end());
    std::fill(
        checked.begin() + static_cast<std::ptrdiff_t>(checksum_offset),
        checked.begin() + static_cast<std::ptrdiff_t>(checksum_offset + sizeof(std::uint32_t)),
        std::byte {0}
    );
    return io::crc32(checked) == expected;
}

} // namespace

std::expected<WalCodec::FileHeader, WalError> WalCodec::encode_file_header(
    const WalFileHeader & value
)
{
    if (value.generation == 0) [[unlikely]] {
        return std::unexpected(make_error(
            WalErrorCode::InvalidFormat,
            "WAL generation must be non-zero",
            {
                .operation = WalOperation::Encode,
                .generation = value.generation,
            }
        ));
    }
    auto without_checksum = encode_file_header_bytes(value, 0);
    if (!without_checksum) [[unlikely]] {
        return std::unexpected(std::move(without_checksum.error()));
    }
    auto encoded = encode_file_header_bytes(value, io::crc32(*without_checksum));
    if (!encoded) [[unlikely]] {
        return std::unexpected(std::move(encoded.error()));
    }
    FileHeader result {};
    std::copy(encoded->begin(), encoded->end(), result.begin());
    return result;
}

std::expected<WalFileHeader, WalError> WalCodec::decode_file_header(
    std::span<const std::byte> bytes
)
{
    if (bytes.size() != FileHeaderSize) [[unlikely]] {
        return std::unexpected(
            make_error(WalErrorCode::InvalidFormat, "Invalid WAL file header size")
        );
    }
    io::BufferByteReader buffer(bytes);
    io::LittleEndianBinaryReader reader(
        buffer,
        {.max_total_bytes = bytes.size(), .max_string_bytes = 0}
    );
    auto magic = reader.read_u32();
    auto version = reader.read_u16();
    auto header_size = reader.read_u16();
    auto generation = reader.read_u64();
    auto checkpoint = reader.read_u64();
    auto checksum = reader.read_u32();
    auto reserved = reader.read_u32();
    if (!magic || !version || !header_size || !generation || !checkpoint || !checksum || !reserved)
        [[unlikely]] {
        return std::unexpected(
            make_error(WalErrorCode::InvalidFormat, "Truncated WAL file header")
        );
    }
    if (*magic != FileMagic) [[unlikely]] {
        return std::unexpected(make_error(WalErrorCode::InvalidFormat, "Invalid WAL file magic"));
    }
    if (*version != Version) [[unlikely]] {
        return std::unexpected(
            make_error(WalErrorCode::UnsupportedVersion, "Unsupported WAL version")
        );
    }
    if (*header_size != FileHeaderSize || *generation == 0 || *reserved != 0) [[unlikely]] {
        return std::unexpected(
            make_error(WalErrorCode::InvalidFormat, "Invalid WAL file header fields")
        );
    }
    if (!checksum_matches(bytes, 24, *checksum)) [[unlikely]] {
        return std::unexpected(
            make_error(WalErrorCode::InvalidFormat, "WAL file header checksum mismatch")
        );
    }
    return WalFileHeader {.generation = *generation, .checkpoint_transaction_id = *checkpoint};
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
        payload.size() > std::numeric_limits<std::uint32_t>::max() ||
        payload.size() > std::numeric_limits<std::size_t>::max() - RecordHeaderSize ||
        payload.size() > std::numeric_limits<std::uint64_t>::max() - RecordHeaderSize)
        [[unlikely]] {
        return std::unexpected(make_error(
            WalErrorCode::InvalidRecord,
            "Invalid WAL record",
            {
                .operation = WalOperation::Encode,
                .transaction_id = transaction_id,
                .lsn = lsn,
            }
        ));
    }
    auto without_checksum = encode_record_bytes(type, lsn, transaction_id, payload, 0);
    if (!without_checksum) [[unlikely]] {
        return std::unexpected(std::move(without_checksum.error()));
    }
    return encode_record_bytes(type, lsn, transaction_id, payload, io::crc32(*without_checksum));
}

std::expected<std::uint64_t, WalError>
WalCodec::decode_record_size(std::span<const std::byte> bytes, transaction::Lsn expected_lsn)
{
    if (bytes.size() < RecordHeaderSize) [[unlikely]] {
        return std::unexpected(invalid_record("WAL record header is truncated", expected_lsn));
    }
    io::BufferByteReader buffer(bytes.first(RecordHeaderSize));
    io::LittleEndianBinaryReader reader(
        buffer,
        {.max_total_bytes = RecordHeaderSize, .max_string_bytes = 0}
    );
    auto magic = reader.read_u32();
    auto version = reader.read_u16();
    auto type = reader.read_u8();
    auto flags = reader.read_u8();
    auto total_size = reader.read_u64();
    auto lsn = reader.read_u64();
    auto transaction_id = reader.read_u64();
    auto payload_size = reader.read_u64();
    auto checksum = reader.read_u32();
    auto reserved = reader.read_u32();
    if (!magic || !version || !type || !flags || !total_size || !lsn || !transaction_id ||
        !payload_size || !checksum || !reserved) [[unlikely]] {
        return std::unexpected(invalid_record("WAL record header is truncated", expected_lsn));
    }
    if (*magic != RecordMagic) [[unlikely]] {
        return std::unexpected(invalid_record("Invalid WAL record magic", expected_lsn));
    }
    if (*version != Version) [[unlikely]] {
        return std::unexpected(make_error(
            WalErrorCode::UnsupportedVersion,
            "Unsupported WAL record version",
            {
                .operation = WalOperation::Decode,
                .lsn = expected_lsn,
            }
        ));
    }
    if (!valid_type(*type) || *flags != 0 || *reserved != 0 || *total_size < RecordHeaderSize ||
        *payload_size != *total_size - RecordHeaderSize || *lsn != expected_lsn ||
        *transaction_id == transaction::InvalidTransactionId) [[unlikely]] {
        return std::unexpected(invalid_record("Invalid WAL record header fields", expected_lsn));
    }
    return *total_size;
}

std::expected<WalRecord, WalError>
WalCodec::decode_record(std::vector<std::byte> bytes, transaction::Lsn expected_lsn)
{
    auto record_size = decode_record_size(bytes, expected_lsn);
    if (!record_size) [[unlikely]] {
        return std::unexpected(std::move(record_size.error()));
    }
    if (*record_size != bytes.size()) [[unlikely]] {
        return std::unexpected(
            invalid_record("WAL record size does not match its buffer", expected_lsn)
        );
    }
    auto stored_checksum = read_checksum(std::span<const std::byte>(bytes).subspan(40, 4));
    if (!stored_checksum) [[unlikely]] {
        return std::unexpected(std::move(stored_checksum.error()));
    }
    if (!checksum_matches(bytes, 40, *stored_checksum)) [[unlikely]] {
        return std::unexpected(make_error(
            WalErrorCode::CorruptedRecord,
            "WAL record checksum mismatch",
            {
                .operation = WalOperation::Decode,
                .lsn = expected_lsn,
            }
        ));
    }
    io::BufferByteReader buffer(bytes);
    io::LittleEndianBinaryReader reader(
        buffer,
        {.max_total_bytes = bytes.size(), .max_string_bytes = 0}
    );
    auto ignored_magic = reader.read_u32();
    auto ignored_version = reader.read_u16();
    auto type = reader.read_u8();
    auto ignored_flags = reader.read_u8();
    auto ignored_total_size = reader.read_u64();
    auto lsn = reader.read_u64();
    auto transaction_id = reader.read_u64();
    auto payload_size = reader.read_u64();
    auto ignored_checksum = reader.read_u32();
    auto ignored_reserved = reader.read_u32();
    if (!ignored_magic || !ignored_version || !type || !ignored_flags || !ignored_total_size ||
        !lsn || !transaction_id || !payload_size || !ignored_checksum || !ignored_reserved)
        [[unlikely]] {
        return std::unexpected(invalid_record("WAL record header is truncated", expected_lsn));
    }
    std::vector<std::byte> payload(*payload_size);
    if (!payload.empty()) {
        auto read = buffer.read_some(payload);
        if (!read || *read != payload.size()) [[unlikely]] {
            return std::unexpected(invalid_record("WAL record payload is truncated", expected_lsn));
        }
    }
    return WalRecord {
        .type = static_cast<WalRecordType>(*type),
        .lsn = *lsn,
        .transaction_id = *transaction_id,
        .payload = std::move(payload),
    };
}

std::expected<std::vector<std::byte>, WalError> WalCodec::encode_file_write(const FileWrite & write)
{
    const auto kind = static_cast<std::uint8_t>(write.target.kind);
    const auto mode = static_cast<std::uint8_t>(write.mode);
    if (kind < static_cast<std::uint8_t>(FileKind::CollectionStore) ||
        kind > static_cast<std::uint8_t>(FileKind::CatalogStore) ||
        mode > static_cast<std::uint8_t>(FileWriteMode::Truncate) ||
        (write.target.kind == FileKind::CatalogStore ? write.target.object_id != 0
                                                     : write.target.object_id == 0) ||
        (write.mode == FileWriteMode::Delete &&
         (!write.after_image.empty() || write.offset != 0)) ||
        (write.mode == FileWriteMode::Truncate && !write.after_image.empty()) ||
        (write.mode == FileWriteMode::Replace && write.offset != 0) ||
        write.after_image.size() >
            std::numeric_limits<std::size_t>::max() - WalCodec::FileWritePayloadHeaderSize ||
        write.offset >
            std::numeric_limits<std::uint64_t>::max() - WalCodec::FileWritePayloadHeaderSize ||
        write.after_image.size() > std::numeric_limits<std::uint64_t>::max() -
                                       WalCodec::FileWritePayloadHeaderSize - write.offset)
        [[unlikely]] {
        return std::unexpected(
            make_error(WalErrorCode::InvalidRecord, "Invalid WAL file-write operation")
        );
    }
    const auto payload_size = WalCodec::FileWritePayloadHeaderSize + write.after_image.size();
    io::BufferByteWriter bytes(payload_size);
    io::LittleEndianBinaryWriter writer(bytes);
    if (auto result = writer.write_u8(kind); !result) [[unlikely]] {
        return std::unexpected(std::move(result.error()));
    }
    if (auto result = writer.write_u8(mode); !result) [[unlikely]] {
        return std::unexpected(std::move(result.error()));
    }
    if (auto result = writer.write_u32(0); !result) [[unlikely]] {
        return std::unexpected(std::move(result.error()));
    }
    if (auto result = writer.write_u16(0); !result) [[unlikely]] {
        return std::unexpected(std::move(result.error()));
    }
    if (auto result = writer.write_u64(write.target.object_id); !result) [[unlikely]] {
        return std::unexpected(std::move(result.error()));
    }
    if (auto result = writer.write_u64(write.offset); !result) [[unlikely]] {
        return std::unexpected(std::move(result.error()));
    }
    if (!write.after_image.empty()) {
        if (auto result = bytes.write_bytes(write.after_image); !result) [[unlikely]] {
            return std::unexpected(std::move(result.error()));
        }
    }
    return bytes.take_bytes();
}

std::expected<FileWrite, WalError> WalCodec::decode_file_write(std::vector<std::byte> payload)
{
    if (payload.size() < WalCodec::FileWritePayloadHeaderSize) [[unlikely]] {
        return std::unexpected(
            make_error(WalErrorCode::CorruptedRecord, "Invalid WAL file-write payload")
        );
    }
    io::BufferByteReader buffer(payload);
    io::LittleEndianBinaryReader reader(
        buffer,
        {.max_total_bytes = payload.size(), .max_string_bytes = 0}
    );
    auto kind_value = reader.read_u8();
    auto mode_value = reader.read_u8();
    auto reserved = reader.read_u32();
    auto reserved_tail = reader.read_u16();
    auto object_id = reader.read_u64();
    auto offset = reader.read_u64();
    if (!kind_value || !mode_value || !reserved || !reserved_tail || !object_id || !offset)
        [[unlikely]] {
        return std::unexpected(
            make_error(WalErrorCode::CorruptedRecord, "Invalid WAL file-write payload")
        );
    }
    if (*reserved != 0 || *reserved_tail != 0 ||
        *kind_value < static_cast<std::uint8_t>(FileKind::CollectionStore) ||
        *kind_value > static_cast<std::uint8_t>(FileKind::CatalogStore) ||
        *mode_value > static_cast<std::uint8_t>(FileWriteMode::Truncate)) [[unlikely]] {
        return std::unexpected(
            make_error(WalErrorCode::CorruptedRecord, "Unknown WAL file target or mode")
        );
    }
    const auto kind = static_cast<FileKind>(*kind_value);
    const auto mode = static_cast<FileWriteMode>(*mode_value);
    if ((mode == FileWriteMode::Replace && *offset != 0) ||
        (mode == FileWriteMode::Delete &&
         (*offset != 0 || payload.size() != WalCodec::FileWritePayloadHeaderSize)) ||
        (mode == FileWriteMode::Truncate &&
         payload.size() != WalCodec::FileWritePayloadHeaderSize) ||
        (kind == FileKind::CatalogStore && *object_id != 0) ||
        (kind != FileKind::CatalogStore && *object_id == 0)) [[unlikely]] {
        return std::unexpected(
            make_error(WalErrorCode::CorruptedRecord, "Invalid WAL file operation")
        );
    }
    std::vector<std::byte> after_image(payload.size() - WalCodec::FileWritePayloadHeaderSize);
    if (!after_image.empty()) {
        auto read = buffer.read_some(after_image);
        if (!read || *read != after_image.size()) [[unlikely]] {
            return std::unexpected(
                make_error(WalErrorCode::CorruptedRecord, "Truncated WAL file-write payload")
            );
        }
    }
    return FileWrite {
        .target = FileTarget {.kind = kind, .object_id = *object_id},
        .offset = *offset,
        .after_image = std::move(after_image),
        .mode = mode,
    };
}

} // namespace litedb::core::wal

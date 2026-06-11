#include "core/persistence/row_log.hpp"

#include <algorithm>
#include <fstream>
#include <sstream>
#include <stdexcept>
#include <utility>

#include "core/persistence/binary_io.hpp"
#include "core/persistence/storage_format.hpp"

namespace litedb::core::persistence
{

namespace
{

constexpr std::uint16_t RowRecordVersion = 1;
constexpr std::uint16_t RowRecordHeaderSize = 28;

storage::StorageError make_error(storage::StorageErrorCode code, std::string message)
{
    return storage::StorageError {code, std::move(message)};
}

void write_file_header(BinaryWriter & writer, std::uint32_t magic)
{
    writer.write_u32(magic);
    writer.write_u16(StorageFormatVersion);
    writer.write_u16(FileHeaderSize);
}

void read_file_header(BinaryReader & reader, std::uint32_t expected_magic)
{
    if (reader.read_u32() != expected_magic) {
        throw std::runtime_error("invalid file magic");
    }
    if (reader.read_u16() != StorageFormatVersion) {
        throw std::runtime_error("unsupported storage format version");
    }
    if (reader.read_u16() < FileHeaderSize) {
        throw std::runtime_error("invalid file header size");
    }
}

void write_record_data(BinaryWriter & writer, const schema::RecordData & data)
{
    writer.write_u32(static_cast<std::uint32_t>(data.values.size()));
    for (const auto & value : data.values) {
        writer.write_value(value);
    }
}

schema::RecordData read_record_data(BinaryReader & reader)
{
    schema::RecordData data;
    const auto count = reader.read_u32();
    data.values.reserve(count);
    for (std::uint32_t index = 0; index < count; ++index) {
        data.values.push_back(reader.read_value());
    }
    return data;
}

std::vector<char> encode_payload(const schema::RecordData * data)
{
    if (data == nullptr) {
        return {};
    }
    std::ostringstream buffer {std::ios::binary};
    BinaryWriter writer {buffer};
    write_record_data(writer, *data);
    const auto text = buffer.str();
    return {text.begin(), text.end()};
}

} // namespace

RowLog::RowLog(std::filesystem::path path, common::CollectionId collection_id)
    : path_(std::move(path))
    , collection_id_(collection_id)
{
}

std::expected<RowLogReplay, storage::StorageError> RowLog::replay_or_create() const
{
    try {
        std::filesystem::create_directories(path_.parent_path());
        if (!std::filesystem::exists(path_)) {
            std::ofstream out {path_, std::ios::binary | std::ios::trunc};
            if (!out) {
                return std::unexpected(make_error(storage::StorageErrorCode::IoError, "Failed to create row log"));
            }
            BinaryWriter writer {out};
            write_file_header(writer, RowsMagic);
            writer.write_u64(collection_id_);
            writer.write_u64(1);
            out.flush();
            if (!out) {
                return std::unexpected(make_error(storage::StorageErrorCode::IoError, "Failed to flush row log"));
            }
            return RowLogReplay {};
        }

        std::ifstream in {path_, std::ios::binary};
        if (!in) {
            return std::unexpected(make_error(storage::StorageErrorCode::IoError, "Failed to open row log"));
        }
        BinaryReader reader {in};
        read_file_header(reader, RowsMagic);
        if (reader.read_u64() != collection_id_) {
            return std::unexpected(make_error(storage::StorageErrorCode::InvalidStorageFormat, "Row log collection id mismatch"));
        }

        RowLogReplay replay;
        replay.next_record_id = reader.read_u64();
        common::RecordId max_record_id = 0;

        for (;;) {
            std::uint32_t magic = 0;
            if (!reader.try_read_u32(magic)) {
                break;
            }

            std::uint8_t rest[RowRecordHeaderSize - sizeof(std::uint32_t)] {};
            if (!reader.try_read_bytes(rest, sizeof(rest))) {
                break;
            }
            std::istringstream header_stream {
                std::string(reinterpret_cast<const char *>(rest), sizeof(rest)),
                std::ios::binary
            };
            BinaryReader header {header_stream};

            if (magic != RowRecordMagic) {
                return std::unexpected(make_error(storage::StorageErrorCode::InvalidStorageFormat, "Invalid row record magic"));
            }

            const auto version = header.read_u16();
            const auto header_size = header.read_u16();
            const auto operation = static_cast<RowLogOperation>(header.read_u8());
            std::uint8_t reserved[7] {};
            if (!header.try_read_bytes(reserved, sizeof(reserved))) {
                return std::unexpected(make_error(storage::StorageErrorCode::InvalidStorageFormat, "Invalid row record header"));
            }
            const auto record_id = header.read_u64();
            const auto payload_size = header.read_u32();

            if (version != RowRecordVersion || header_size != RowRecordHeaderSize) {
                return std::unexpected(make_error(storage::StorageErrorCode::InvalidStorageFormat, "Unsupported row record version"));
            }
            if (operation != RowLogOperation::Insert && operation != RowLogOperation::Update && operation != RowLogOperation::Delete) {
                return std::unexpected(make_error(storage::StorageErrorCode::InvalidStorageFormat, "Invalid row record operation"));
            }

            std::vector<char> payload(payload_size);
            if (payload_size != 0 && !reader.try_read_bytes(payload.data(), payload.size())) {
                break;
            }

            RowLogRecord record {
                .operation = operation,
                .record_id = record_id,
                .data = {},
            };
            if (operation != RowLogOperation::Delete) {
                std::istringstream payload_stream {std::string(payload.begin(), payload.end()), std::ios::binary};
                BinaryReader payload_reader {payload_stream};
                record.data = read_record_data(payload_reader);
            }
            max_record_id = std::max(max_record_id, record_id);
            replay.records.push_back(std::move(record));
        }

        replay.next_record_id = std::max(replay.next_record_id, max_record_id + 1);
        return replay;
    } catch (const std::exception & exception) {
        return std::unexpected(make_error(storage::StorageErrorCode::InvalidStorageFormat, exception.what()));
    }
}

std::expected<void, storage::StorageError> RowLog::append_insert(
    common::RecordId record_id,
    const schema::RecordData & data
) const
{
    return append(RowLogOperation::Insert, record_id, &data);
}

std::expected<void, storage::StorageError> RowLog::append_update(
    common::RecordId record_id,
    const schema::RecordData & data
) const
{
    return append(RowLogOperation::Update, record_id, &data);
}

std::expected<void, storage::StorageError> RowLog::append_delete(common::RecordId record_id) const
{
    return append(RowLogOperation::Delete, record_id, nullptr);
}

std::expected<void, storage::StorageError> RowLog::mark_dropped() const
{
    try {
        if (!std::filesystem::exists(path_)) {
            return {};
        }
        std::filesystem::path dropped = path_;
        dropped += ".dropped";
        std::error_code ignored;
        std::filesystem::remove(dropped, ignored);
        std::filesystem::rename(path_, dropped);
        return {};
    } catch (const std::exception & exception) {
        return std::unexpected(make_error(storage::StorageErrorCode::IoError, exception.what()));
    }
}

const std::filesystem::path & RowLog::path() const noexcept
{
    return path_;
}

std::expected<void, storage::StorageError> RowLog::append(
    RowLogOperation operation,
    common::RecordId record_id,
    const schema::RecordData * data
) const
{
    try {
        const auto payload = encode_payload(data);
        std::ofstream out {path_, std::ios::binary | std::ios::app};
        if (!out) {
            return std::unexpected(make_error(storage::StorageErrorCode::IoError, "Failed to open row log for append"));
        }

        BinaryWriter writer {out};
        writer.write_u32(RowRecordMagic);
        writer.write_u16(RowRecordVersion);
        writer.write_u16(RowRecordHeaderSize);
        writer.write_u8(static_cast<std::uint8_t>(operation));
        for (std::size_t index = 0; index < 7; ++index) {
            writer.write_u8(0);
        }
        writer.write_u64(record_id);
        writer.write_u32(static_cast<std::uint32_t>(payload.size()));
        if (!payload.empty()) {
            out.write(payload.data(), static_cast<std::streamsize>(payload.size()));
        }
        out.flush();
        if (!out) {
            return std::unexpected(make_error(storage::StorageErrorCode::IoError, "Failed to append row log record"));
        }
        return {};
    } catch (const std::exception & exception) {
        return std::unexpected(make_error(storage::StorageErrorCode::IoError, exception.what()));
    }
}

} // namespace litedb::core::persistence

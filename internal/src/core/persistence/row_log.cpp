#include "core/persistence/row_log.hpp"

#include <algorithm>
#include <stdexcept>
#include <utility>

#include "core/io/binary_io.hpp"
#include "core/io/buffer_byte_reader.hpp"
#include "core/io/buffer_byte_writer.hpp"
#include "core/io/file_byte_reader.hpp"
#include "core/io/file_byte_writer.hpp"
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

storage::StorageError from_filesystem_error(filesystem::FileSystemError error)
{
    return storage::StorageError {
        .code = storage::StorageErrorCode::IoError,
        .message = std::move(error.message),
    };
}

void require_io(std::expected<void, io::IoError> result)
{
    if (!result.has_value()) {
        throw std::runtime_error(result.error().message);
    }
}

template <typename T>
T require_io(std::expected<T, io::IoError> result)
{
    if (!result.has_value()) {
        throw std::runtime_error(result.error().message);
    }
    return std::move(result.value());
}

void write_file_header(io::BinaryWriter & writer, std::uint32_t magic)
{
    require_io(writer.write_u32(magic));
    require_io(writer.write_u16(StorageFormatVersion));
    require_io(writer.write_u16(FileHeaderSize));
}

void read_file_header(io::BinaryReader & reader, std::uint32_t expected_magic)
{
    if (require_io(reader.read_u32()) != expected_magic) {
        throw std::runtime_error("invalid file magic");
    }
    if (require_io(reader.read_u16()) != StorageFormatVersion) {
        throw std::runtime_error("unsupported storage format version");
    }
    if (require_io(reader.read_u16()) < FileHeaderSize) {
        throw std::runtime_error("invalid file header size");
    }
}

void write_record_data(io::BinaryWriter & writer, const schema::RecordData & data)
{
    require_io(writer.write_u32(static_cast<std::uint32_t>(data.values.size())));
    for (const auto & value : data.values) {
        require_io(writer.write_value(value));
    }
}

schema::RecordData read_record_data(io::BinaryReader & reader)
{
    schema::RecordData data;
    const auto count = require_io(reader.read_u32());
    data.values.reserve(count);
    for (std::uint32_t index = 0; index < count; ++index) {
        data.values.push_back(require_io(reader.read_value()));
    }
    return data;
}

std::vector<std::byte> encode_payload(const schema::RecordData * data)
{
    if (data == nullptr) {
        return {};
    }
    io::BufferByteWriter buffer;
    io::BinaryWriter writer {buffer};
    write_record_data(writer, *data);
    return buffer.take_bytes();
}

} // namespace

RowLog::RowLog(std::filesystem::path path, common::CollectionId collection_id, filesystem::FileSystem & filesystem)
    : path_(std::move(path))
    , collection_id_(collection_id)
    , filesystem_(&filesystem)
{
}

std::expected<RowLogReplay, storage::StorageError> RowLog::replay_or_create() const
{
    try {
        auto created = filesystem_->create_dir_all(path_.parent_path());
        if (!created.has_value()) {
            return std::unexpected(from_filesystem_error(std::move(created.error())));
        }
        auto exists = filesystem_->exists(path_);
        if (!exists.has_value()) {
            return std::unexpected(from_filesystem_error(std::move(exists.error())));
        }
        if (!exists.value()) {
            auto file = filesystem_->open(
                path_,
                filesystem::backend::FileOpenOptions {
                    .access = filesystem::backend::FileAccess::ReadWrite,
                    .create_mode = filesystem::backend::FileCreateMode::CreateOrTruncate,
                }
            );
            if (!file.has_value()) {
                return std::unexpected(from_filesystem_error(std::move(file.error())));
            }
            io::FileByteWriter byte_writer {file.value()};
            io::BinaryWriter writer {byte_writer};
            write_file_header(writer, RowsMagic);
            require_io(writer.write_u64(collection_id_));
            require_io(writer.write_u64(1));
            auto synced = file->sync_all();
            if (!synced.has_value()) {
                return std::unexpected(from_filesystem_error(std::move(synced.error())));
            }
            auto closed = file->close();
            if (!closed.has_value()) {
                return std::unexpected(from_filesystem_error(std::move(closed.error())));
            }
            return RowLogReplay {};
        }

        auto file = filesystem_->open(
            path_,
            filesystem::backend::FileOpenOptions {
                .access = filesystem::backend::FileAccess::ReadOnly,
                .create_mode = filesystem::backend::FileCreateMode::OpenExisting,
            }
        );
        if (!file.has_value()) {
            return std::unexpected(from_filesystem_error(std::move(file.error())));
        }
        io::FileByteReader byte_reader {file.value()};
        io::BinaryReader reader {byte_reader};
        read_file_header(reader, RowsMagic);
        if (require_io(reader.read_u64()) != collection_id_) {
            return std::unexpected(make_error(storage::StorageErrorCode::InvalidStorageFormat, "Row log collection id mismatch"));
        }

        RowLogReplay replay;
        replay.next_record_id = require_io(reader.read_u64());
        common::RecordId max_record_id = 0;

        for (;;) {
            std::uint32_t magic = 0;
            auto magic_read = reader.read_u32(magic);
            if (!magic_read.has_value()) {
                return std::unexpected(make_error(storage::StorageErrorCode::InvalidStorageFormat, std::move(magic_read.error().message)));
            }
            if (!magic_read.value()) {
                break;
            }

            std::uint8_t rest[RowRecordHeaderSize - sizeof(std::uint32_t)] {};
            auto rest_read = reader.read_bytes(rest, sizeof(rest));
            if (!rest_read.has_value()) {
                return std::unexpected(make_error(storage::StorageErrorCode::InvalidStorageFormat, std::move(rest_read.error().message)));
            }
            if (!rest_read.value()) {
                break;
            }
            auto rest_bytes = std::as_bytes(std::span {rest});
            io::BufferByteReader header_buffer {rest_bytes};
            io::BinaryReader header {header_buffer};

            if (magic != RowRecordMagic) {
                return std::unexpected(make_error(storage::StorageErrorCode::InvalidStorageFormat, "Invalid row record magic"));
            }

            const auto version = require_io(header.read_u16());
            const auto header_size = require_io(header.read_u16());
            const auto operation = static_cast<RowLogOperation>(require_io(header.read_u8()));
            std::uint8_t reserved[7] {};
            auto reserved_read = header.read_bytes(reserved, sizeof(reserved));
            if (!reserved_read.has_value()) {
                return std::unexpected(make_error(storage::StorageErrorCode::InvalidStorageFormat, std::move(reserved_read.error().message)));
            }
            if (!reserved_read.value()) {
                return std::unexpected(make_error(storage::StorageErrorCode::InvalidStorageFormat, "Invalid row record header"));
            }
            const auto record_id = require_io(header.read_u64());
            const auto payload_size = require_io(header.read_u32());

            if (version != RowRecordVersion || header_size != RowRecordHeaderSize) {
                return std::unexpected(make_error(storage::StorageErrorCode::InvalidStorageFormat, "Unsupported row record version"));
            }
            if (operation != RowLogOperation::Insert && operation != RowLogOperation::Update && operation != RowLogOperation::Delete) {
                return std::unexpected(make_error(storage::StorageErrorCode::InvalidStorageFormat, "Invalid row record operation"));
            }

            std::vector<char> payload(payload_size);
            if (payload_size != 0) {
                auto payload_read = reader.read_bytes(payload.data(), payload.size());
                if (!payload_read.has_value()) {
                    return std::unexpected(make_error(storage::StorageErrorCode::InvalidStorageFormat, std::move(payload_read.error().message)));
                }
                if (!payload_read.value()) {
                    break;
                }
            }

            RowLogRecord record {
                .operation = operation,
                .record_id = record_id,
                .data = {},
            };
            if (operation != RowLogOperation::Delete) {
                io::BufferByteReader payload_buffer {std::as_bytes(std::span {payload})};
                io::BinaryReader payload_reader {payload_buffer};
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
        auto exists = filesystem_->exists(path_);
        if (!exists.has_value()) {
            return std::unexpected(from_filesystem_error(std::move(exists.error())));
        }
        if (!exists.value()) {
            return {};
        }
        std::filesystem::path dropped = path_;
        dropped += ".dropped";
        auto dropped_exists = filesystem_->exists(dropped);
        if (!dropped_exists.has_value()) {
            return std::unexpected(from_filesystem_error(std::move(dropped_exists.error())));
        }
        if (dropped_exists.value()) {
            auto removed = filesystem_->remove(dropped);
            if (!removed.has_value()) {
                return std::unexpected(from_filesystem_error(std::move(removed.error())));
            }
        }
        auto renamed = filesystem_->rename(path_, dropped);
        if (!renamed.has_value()) {
            return std::unexpected(from_filesystem_error(std::move(renamed.error())));
        }
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
        io::BufferByteWriter record_buffer;
        io::BinaryWriter record_writer {record_buffer};

        require_io(record_writer.write_u32(RowRecordMagic));
        require_io(record_writer.write_u16(RowRecordVersion));
        require_io(record_writer.write_u16(RowRecordHeaderSize));
        require_io(record_writer.write_u8(static_cast<std::uint8_t>(operation)));
        for (std::size_t index = 0; index < 7; ++index) {
            require_io(record_writer.write_u8(0));
        }
        require_io(record_writer.write_u64(record_id));
        require_io(record_writer.write_u32(static_cast<std::uint32_t>(payload.size())));
        if (!payload.empty()) {
            auto payload_written = record_buffer.write_bytes(payload);
            if (!payload_written.has_value()) {
                return std::unexpected(make_error(storage::StorageErrorCode::IoError, std::move(payload_written.error().message)));
            }
        }

        auto file = filesystem_->open(
            path_,
            filesystem::backend::FileOpenOptions {
                .access = filesystem::backend::FileAccess::WriteOnly,
                .create_mode = filesystem::backend::FileCreateMode::OpenExisting,
            }
        );
        if (!file.has_value()) {
            return std::unexpected(from_filesystem_error(std::move(file.error())));
        }
        io::FileByteAppender appender {file.value()};
        auto appended = appender.write_bytes(record_buffer.bytes());
        if (!appended.has_value()) {
            return std::unexpected(make_error(storage::StorageErrorCode::IoError, std::move(appended.error().message)));
        }
        auto synced = file->sync_data();
        if (!synced.has_value()) {
            return std::unexpected(from_filesystem_error(std::move(synced.error())));
        }
        auto closed = file->close();
        if (!closed.has_value()) {
            return std::unexpected(from_filesystem_error(std::move(closed.error())));
        }
        return {};
    } catch (const std::exception & exception) {
        return std::unexpected(make_error(storage::StorageErrorCode::IoError, exception.what()));
    }
}

} // namespace litedb::core::persistence

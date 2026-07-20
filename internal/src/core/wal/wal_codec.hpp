#pragma once

#include <array>
#include <cstddef>
#include <cstdint>
#include <expected>
#include <span>
#include <vector>

#include "core/wal/wal_error.hpp"
#include "core/wal/wal_types.hpp"

namespace litedb::core::wal
{

class WalCodec final
{
public:
    static constexpr std::size_t FileHeaderSize = 32;
    static constexpr std::size_t RecordHeaderSize = 48;
    using FileHeader = std::array<std::byte, FileHeaderSize>;

    [[nodiscard]] static FileHeader encode_file_header() noexcept;
    [[nodiscard]] static std::expected<void, WalError> decode_file_header(std::span<const std::byte> bytes);

    [[nodiscard]] static std::expected<std::vector<std::byte>, WalError> encode_record(
        WalRecordType type,
        transaction::Lsn lsn,
        transaction::TransactionId transaction_id,
        std::span<const std::byte> payload
    );

    [[nodiscard]] static std::expected<WalRecord, WalError> decode_record(
        std::span<const std::byte> bytes,
        transaction::Lsn expected_lsn
    );

    [[nodiscard]] static std::vector<std::byte> encode_file_write(const FileWrite & write);
    [[nodiscard]] static std::expected<FileWrite, WalError> decode_file_write(std::span<const std::byte> payload);
};

} // namespace litedb::core::wal

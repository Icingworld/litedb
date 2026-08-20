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

// WAL 编解码器
class WalCodec final
{
public:
    static constexpr std::size_t FileHeaderSize = 32;
    static constexpr std::size_t RecordHeaderSize = 48;

    using FileHeader = std::array<std::byte, FileHeaderSize>;

public:
    // 编码文件头
    [[nodiscard]]
    static std::expected<FileHeader, WalError> encode_file_header(const WalFileHeader & header);

    // 解码并校验文件头
    [[nodiscard]]
    static std::expected<WalFileHeader, WalError> decode_file_header(
        std::span<const std::byte> bytes
    );

    // 编码 WAL 记录
    [[nodiscard]]
    static std::expected<std::vector<std::byte>, WalError> encode_record(
        WalRecordType type,
        transaction::Lsn lsn,
        transaction::TransactionId transaction_id,
        std::span<const std::byte> payload
    );

    // 解码记录头并返回完整记录尺寸。
    // WAL 存储只需要通过该接口确定下一次读取的边界，避免依赖持久化字段偏移。
    [[nodiscard]]
    static std::expected<std::uint64_t, WalError>
    decode_record_size(std::span<const std::byte> bytes, transaction::Lsn expected_lsn);

    // 解码 WAL 记录
    [[nodiscard]]
    static std::expected<WalRecord, WalError>
    decode_record(std::vector<std::byte> bytes, transaction::Lsn expected_lsn);

    // 编码文件写入负载
    [[nodiscard]]
    static std::expected<std::vector<std::byte>, WalError> encode_file_write(
        const FileWrite & write
    );

    // 解码文件写入负载
    [[nodiscard]]
    static std::expected<FileWrite, WalError> decode_file_write(std::vector<std::byte> payload);
};

} // namespace litedb::core::wal

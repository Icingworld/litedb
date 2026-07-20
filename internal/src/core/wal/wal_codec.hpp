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

/**
 * @brief WAL 编解码器
 */
class WalCodec final
{
public:
    static constexpr std::size_t FileHeaderSize = 32;
    static constexpr std::size_t RecordHeaderSize = 48;

    using FileHeader = std::array<std::byte, FileHeaderSize>;

public:
    /**
     * @brief 编码文件头
     * @return 文件头字节
     */
    [[nodiscard]]
    static FileHeader encode_file_header() noexcept;

    /**
     * @brief 解码并校验文件头
     * @param bytes 文件头字节
     * @return 结果
     */
    [[nodiscard]]
    static std::expected<void, WalError> decode_file_header(std::span<const std::byte> bytes);

    /**
     * @brief 编码 WAL 记录
     * @param type 记录类型
     * @param lsn 日志序列号
     * @param transaction_id 事务 ID
     * @param payload 负载数据
     * @return 编码后的记录
     */
    [[nodiscard]]
    static std::expected<std::vector<std::byte>, WalError> encode_record(
        WalRecordType type,
        transaction::Lsn lsn,
        transaction::TransactionId transaction_id,
        std::span<const std::byte> payload
    );

    /**
     * @brief 解码 WAL 记录
     * @param bytes 记录字节
     * @param expected_lsn 期望的日志序列号
     * @return 解码后的记录
     */
    [[nodiscard]]
    static std::expected<WalRecord, WalError> decode_record(
        std::span<const std::byte> bytes,
        transaction::Lsn expected_lsn
    );

    /**
     * @brief 编码文件写入负载
     * @param write 文件写入记录
     * @return 编码后的负载
     */
    [[nodiscard]]
    static std::vector<std::byte> encode_file_write(const FileWrite & write);

    /**
     * @brief 解码文件写入负载
     * @param payload 负载数据
     * @return 文件写入记录
     */
    [[nodiscard]]
    static std::expected<FileWrite, WalError> decode_file_write(std::span<const std::byte> payload);
};

} // namespace litedb::core::wal

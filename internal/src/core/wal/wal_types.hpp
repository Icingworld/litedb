#pragma once

#include <cstddef>
#include <cstdint>
#include <limits>
#include <vector>

#include "core/transaction/transaction_id.hpp"

namespace litedb::core::wal
{

struct WalFileHeader
{
    std::uint64_t generation {1};
    transaction::TransactionId checkpoint_transaction_id {transaction::InvalidTransactionId};
};

/**
 * @brief 文件目标类型
 */
enum class FileKind : std::uint8_t
{
    CollectionStore = 1,    // 集合存储
    ScalarIndex = 2,        // 标量索引
    VectorIndex = 3,        // 向量索引
    MetaStore = 4,          // 元数据快照
};

enum class FileWriteMode : std::uint8_t
{
    Overwrite = 0,          // 覆盖指定范围
    Replace = 1,            // 以 after-image 完整替换文件
    Delete = 2,             // 删除文件
    Truncate = 3,           // 将文件调整为 offset 指定的长度
};

/**
 * @brief WAL 扫描与恢复资源预算
 *
 * 所有限制都可以由嵌入方显式放宽，但默认值会阻止损坏日志触发无界分配。
 */
struct WalDecodeLimits
{
    std::uint64_t max_record_size_bytes {512ULL * 1024ULL * 1024ULL};
    std::uint64_t max_scan_size_bytes {4ULL * 1024ULL * 1024ULL * 1024ULL};
    std::size_t max_record_count {2'000'000};
};

/**
 * @brief 文件写入目标
 */
struct FileTarget
{
    FileKind kind;           // 文件类型
    std::uint64_t object_id; // 对象 ID

    friend bool operator==(const FileTarget &, const FileTarget &) = default;
};

/**
 * @brief 文件写入记录
 */
struct FileWrite
{
    FileTarget target;                   // 文件目标
    std::uint64_t offset;                // 偏移量
    std::vector<std::byte> after_image;  // 修改后的数据
    FileWriteMode mode {FileWriteMode::Overwrite}; // 文件操作模式
};

/**
 * @brief WAL 记录类型
 */
enum class WalRecordType : std::uint8_t
{
    Begin = 1,        // 开始
    FileWrite = 2,    // 文件写入
    Commit = 3,       // 提交
};

/**
 * @brief WAL 记录
 */
struct WalRecord
{
    WalRecordType type;                         // 记录类型
    transaction::Lsn lsn;                       // 日志序列号
    transaction::TransactionId transaction_id;  // 事务 ID
    std::vector<std::byte> payload;             // 负载数据
};

/**
 * @brief WAL 扫描结果
 */
struct WalScanResult
{
    std::vector<WalRecord> records;             // 记录
    std::uint64_t valid_size;                   // 有效大小
    bool truncated_tail {false};                // 是否截断尾部
    transaction::TransactionId maximum_transaction_id {transaction::InvalidTransactionId}; // 最大事务 ID
};

} // namespace litedb::core::wal

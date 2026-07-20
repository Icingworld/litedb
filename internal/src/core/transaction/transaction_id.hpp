#pragma once

#include <cstdint>

namespace litedb::core::transaction
{

/**
 * @brief 事务 ID 类型
 */
using TransactionId = std::uint64_t;

/**
 * @brief WAL 日志序列号
 * @details 第一版使用 WAL record 在文件中的起始偏移。
 */
using Lsn = std::uint64_t;

/**
 * @brief 无效的事务 ID
 */
inline constexpr TransactionId InvalidTransactionId = 0;

/**
 * @brief 无效的 WAL 日志序列号
 */
inline constexpr Lsn InvalidLsn = 0;

} // namespace litedb::core::transaction

#pragma once

#include <cstdint>
#include <optional>
#include <string>
#include <utility>

#include "core/error/error.hpp"
#include "core/transaction/transaction_id.hpp"

namespace litedb::core::transaction
{

/**
 * @brief 事务错误码枚举
 */
enum class TransactionErrorCode : std::uint8_t
{
    InvalidState,              // 无效状态
    RollbackOnly,              // 回滚状态
    PrepareFailed,             // 准备失败
    WalError,                  // WAL 写入失败
    ApplyFailed,               // 应用失败
    CommittedApplyFailed,      // 提交应用失败
    RecoveryRequired,          // 需要恢复
    FaultInjected,             // 测试故障注入
};

/**
 * @brief 事务操作
 */
enum class TransactionOperation : std::uint8_t
{
    Begin,
    Stage,
    Prepare,
    AppendWal,
    FlushWal,
    Apply,
    Reload,
    Abort,
    Checkpoint,
};

/**
 * @brief 事务错误上下文
 */
struct TransactionErrorContext
{
    TransactionOperation operation {TransactionOperation::Begin};
    TransactionId transaction_id {InvalidTransactionId};
    std::optional<std::uint16_t> source_code;
};

using TransactionError = error::Error;

} // namespace litedb::core::transaction

namespace litedb::core::error
{

template <>
struct ErrorTraits<transaction::TransactionErrorCode>
{
    static constexpr ErrorCategory category = ErrorCategory::Transaction;
};

} // namespace litedb::core::error

namespace litedb::core::transaction
{

[[nodiscard]]
inline TransactionError make_error(
    TransactionErrorCode code,
    std::string message,
    TransactionErrorContext context = {}
)
{
    return TransactionError {code, message, std::move(context)};
}

[[nodiscard]]
inline TransactionError make_error(
    TransactionErrorCode code,
    std::string message,
    TransactionErrorContext context,
    error::Error cause
)
{
    context.source_code = cause.encode_code();
    return TransactionError {code, message, std::move(context), std::move(cause)};
}

} // namespace litedb::core::transaction

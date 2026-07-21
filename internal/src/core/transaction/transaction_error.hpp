#pragma once

#include <string>

#include "core/transaction/transaction_id.hpp"

namespace litedb::core::transaction
{

/**
 * @brief 事务错误码枚举
 */
enum class TransactionErrorCode
{
    InvalidState,              ///< 无效状态
    RollbackOnly,              ///< 回滚状态
    PrepareFailed,             ///< 准备失败
    WalError,                  ///< WAL 写入失败
    ApplyFailed,               ///< 应用失败
    CommittedApplyFailed,      ///< 提交应用失败
    RecoveryRequired,          ///< 需要恢复
    FaultInjected,             ///< 测试故障注入
};

/**
 * @brief 事务错误信息
 */
struct TransactionError
{
    TransactionErrorCode code;       ///< 错误码
    TransactionId transaction_id;    ///< 事务 ID
    std::string message;             ///< 错误信息
};

} // namespace litedb::core::transaction

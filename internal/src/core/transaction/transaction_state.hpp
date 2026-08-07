#pragma once

namespace litedb::core::transaction
{

/**
 * @brief 事务状态枚举
 */
enum class TransactionState
{
    Active,           // 活跃状态
    Preparing,        // 正在准备持久化写集合
    Committing,       // 正在提交状态
    Committed,        // 已提交状态
    Aborting,         // 正在回滚状态
    Aborted,          // 已回滚状态
};

/**
 * @brief 判断事务状态是否可以转换
 * @param from 起始状态
 * @param to 目标状态
 * @return 是否可以转换
 * @details 定义了状态转换表，用于判断事务状态是否可以转换
 */
[[nodiscard]]
bool can_transition(TransactionState from, TransactionState to) noexcept;

} // namespace litedb::core::transaction

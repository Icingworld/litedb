#pragma once

#include <mutex>
#include <optional>
#include <string>
#include <vector>

#include "core/common/ids.hpp"
#include "core/schema/record.hpp"
#include "core/transaction/transaction_id.hpp"
#include "core/transaction/transaction_state.hpp"

namespace litedb::core::transaction
{

class TransactionManager;

/**
 * @brief 隔离级别枚举
 * @details 第一版只支持最高等级的可序列化隔离级别
 */
enum class IsolationLevel
{
    // ReadUncommitted,  ///< 读未提交
    // ReadCommitted,    ///< 读已提交
    // RepeatableRead,   ///< 可重复读
    Serializable,     ///< 可序列化
};

/**
 * @brief 行变更类型枚举
 */
enum class RowMutationKind
{
    Insert,           ///< 插入
    Update,           ///< 更新
    Delete,           ///< 删除
};

/**
 * @brief 行变更结构体
 */
struct RowMutation
{
    RowMutationKind kind;                        ///< 变更类型
    common::CollectionId collection_id;          ///< 集合 ID
    common::RecordId record_id;                  ///< 记录 ID
    std::optional<schema::RecordData> before;    ///< 变更前的记录数据
    std::optional<schema::RecordData> after;     ///< 变更后的记录数据
};

/**
 * @brief 事务失败信息结构体
 */
struct TransactionFailure
{
    std::string message;     ///< 错误信息
};

/**
 * @brief 事务上下文
 * @details 保存单个事务的状态、LSN 区间与写集合
 */
class TransactionContext final
{
public:
    explicit TransactionContext(TransactionId id) noexcept;

    TransactionContext(const TransactionContext &) = delete;

    TransactionContext & operator=(const TransactionContext &) = delete;

    TransactionContext(TransactionContext &&) noexcept = default;

    TransactionContext & operator=(TransactionContext &&) noexcept = default;

public:
    /**
     * @brief 获取事务 ID
     * @return 事务 ID
     */
    [[nodiscard]]
    TransactionId id() const noexcept;

    /**
     * @brief 获取事务状态
     * @return 事务状态
     */
    [[nodiscard]]
    TransactionState state() const noexcept;

    /**
     * @brief 获取隔离级别
     * @return 隔离级别
     */
    [[nodiscard]]
    IsolationLevel isolation() const noexcept;

    /**
     * @brief 获取事务首条 WAL 日志序列号
     * @return 首条 LSN
     */
    [[nodiscard]]
    const std::optional<Lsn> & first_lsn() const noexcept;

    /**
     * @brief 获取事务最新 WAL 日志序列号
     * @return 最新 LSN
     */
    [[nodiscard]]
    const std::optional<Lsn> & last_lsn() const noexcept;

    /**
     * @brief 获取事务提交 WAL 日志序列号
     * @return 提交 LSN
     */
    [[nodiscard]]
    const std::optional<Lsn> & commit_lsn() const noexcept;

    /**
     * @brief 获取事务写集合
     * @return 写集合
     */
    [[nodiscard]]
    const std::vector<RowMutation> & write_set() const noexcept;

    /**
     * @brief 判断事务是否只能回滚
     * @return 是否只能回滚
     */
    [[nodiscard]]
    bool rollback_only() const noexcept;

    /**
     * @brief 获取事务失败信息
     * @return 失败信息
     */
    [[nodiscard]]
    const std::optional<TransactionFailure> & failure() const noexcept;

    /**
     * @brief 暂存一行变更到写集合
     * @param mutation 行变更
     */
    void stage(RowMutation mutation);

private:
    friend class TransactionManager;

    /**
     * @brief 尝试转换事务状态
     * @param next 目标状态
     * @return 是否转换成功
     */
    [[nodiscard]]
    bool transition_to(TransactionState next) noexcept;

    /**
     * @brief 记录一条 WAL 日志序列号
     * @param lsn WAL 日志序列号
     */
    void note_lsn(Lsn lsn) noexcept;

    /**
     * @brief 记录事务提交 WAL 日志序列号
     * @param lsn 提交 LSN
     */
    void note_commit_lsn(Lsn lsn) noexcept;

    /**
     * @brief 将事务标记为只能回滚
     * @param message 失败信息
     */
    void mark_rollback_only(std::string message);

    /**
     * @brief 释放事务持有的单写者锁
     */
    void release_writer_guard() noexcept;

private:
    TransactionId id_;                                           ///< 事务 ID
    TransactionState state_ {TransactionState::Active};          ///< 事务状态
    IsolationLevel isolation_ {IsolationLevel::Serializable};    ///< 隔离级别
    std::optional<Lsn> first_lsn_;                               ///< 首条 LSN
    std::optional<Lsn> last_lsn_;                                ///< 最新 LSN
    std::optional<Lsn> commit_lsn_;                              ///< 提交 LSN
    std::vector<RowMutation> write_set_;                         ///< 写集合
    bool rollback_only_ {false};                                 ///< 是否只能回滚
    std::optional<TransactionFailure> failure_;                  ///< 失败信息
    std::unique_lock<std::mutex> writer_guard_;                  ///< 单写者生命周期锁
    TransactionManager * owner_ {nullptr};                       ///< 创建该事务的管理器
};

} // namespace litedb::core::transaction

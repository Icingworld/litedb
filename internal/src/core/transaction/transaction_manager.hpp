#pragma once

#include <atomic>
#include <cstdint>
#include <expected>
#include <filesystem>
#include <functional>
#include <mutex>
#include <string>

#include "core/filesystem/filesystem.hpp"
#include "core/index/index_engine.hpp"
#include "core/meta/meta_engine.hpp"
#include "core/storage/storage_engine.hpp"
#include "core/transaction/transaction_context.hpp"
#include "core/transaction/transaction_error.hpp"
#include "core/vindex/vector_index_engine.hpp"
#include "core/wal/file_write_batch.hpp"
#include "core/wal/wal_store.hpp"

namespace litedb::core::transaction
{

enum class CommitStage
{
    AfterPrepare,
    AfterWalBegin,
    AfterWalWrites,
    AfterWalCommitAppend,
    AfterWalCommitFlush,
    AfterApply,
    AfterRuntimeReload,
};

using CommitStageHook = std::function<bool(CommitStage, TransactionId)>;

struct TransactionOptions
{
    CommitStageHook commit_stage_hook;
};

struct TransactionMetrics
{
    std::uint64_t started_transactions {0};
    std::uint64_t committed_transactions {0};
    std::uint64_t aborted_transactions {0};
    std::uint64_t failed_commits {0};
    std::uint64_t total_commit_duration_us {0};
    std::uint64_t last_commit_duration_us {0};
    std::uint64_t maximum_commit_duration_us {0};
    std::uint64_t wal_size_bytes {0};
};

/**
 * @brief 事务管理器
 * @details 负责隐式事务的开启、写集合暂存、提交与回滚
 */
class TransactionManager final
{
public:
    TransactionManager(
        std::filesystem::path data_directory,
        filesystem::FileSystem & filesystem,
        meta::MetaEngine & catalog,
        storage::StorageEngine & storage,
        index::IndexEngine & index_engine,
        vindex::VectorIndexEngine & vector_index_engine,
        wal::WalStore & wal,
        TransactionId maximum_recovered_transaction_id,
        TransactionOptions options = {}
    ) noexcept;

public:
    /**
     * @brief 开启一个隐式事务
     * @return 成功时返回事务上下文，失败时返回错误
     */
    [[nodiscard]]
    std::expected<TransactionContext, TransactionError> begin_implicit();

    /**
     * @brief 暂存一次插入变更
     * @param transaction 事务上下文
     * @param collection_id 集合 ID
     * @param after 插入后的记录数据
     * @return 成功或错误
     */
    [[nodiscard]]
    std::expected<void, TransactionError> stage_insert(
        TransactionContext & transaction,
        common::CollectionId collection_id,
        schema::RecordData after
    );

    /**
     * @brief 暂存一次更新变更
     * @param transaction 事务上下文
     * @param collection_id 集合 ID
     * @param record_id 记录 ID
     * @param before 更新前的记录数据
     * @param after 更新后的记录数据
     * @return 成功或错误
     */
    [[nodiscard]]
    std::expected<void, TransactionError> stage_update(
        TransactionContext & transaction,
        common::CollectionId collection_id,
        common::RecordId record_id,
        schema::RecordData before,
        schema::RecordData after
    );

    /**
     * @brief 暂存一次删除变更
     * @param transaction 事务上下文
     * @param collection_id 集合 ID
     * @param record_id 记录 ID
     * @param before 删除前的记录数据
     * @return 成功或错误
     */
    [[nodiscard]]
    std::expected<void, TransactionError> stage_delete(
        TransactionContext & transaction,
        common::CollectionId collection_id,
        common::RecordId record_id,
        schema::RecordData before
    );

    /**
     * @brief 提交事务
     * @param transaction 事务上下文
     * @return 成功或错误
     */
    [[nodiscard]]
    std::expected<void, TransactionError> commit(TransactionContext & transaction);

    /**
     * @brief 回滚事务
     * @param transaction 事务上下文
     * @return 成功或错误
     */
    [[nodiscard]]
    std::expected<void, TransactionError> abort(TransactionContext & transaction);

    /**
     * @brief 判断数据库是否需要先恢复
     * @return 是否需要恢复
     */
    [[nodiscard]]
    bool recovery_required() const noexcept;

    [[nodiscard]]
    TransactionMetrics metrics() const noexcept;

private:
    /**
     * @brief 在暂存目录中准备事务写集合
     * @param transaction 事务上下文
     * @param staging_directory 暂存目录
     * @return 成功时返回文件写批次，失败时返回错误
     */
    [[nodiscard]]
    std::expected<wal::FileWriteBatch, TransactionError> prepare(
        const TransactionContext & transaction,
        const std::filesystem::path & staging_directory
    );

    /**
     * @brief 重新加载运行时存储与索引
     * @param transaction_id 事务 ID
     * @return 成功或错误
     */
    [[nodiscard]]
    std::expected<void, TransactionError> reload_runtime(TransactionId transaction_id);

    /**
     * @brief 构造事务错误
     * @param code 错误码
     * @param id 事务 ID
     * @param message 错误信息
     * @return 事务错误
     */
    [[nodiscard]]
    TransactionError error(TransactionErrorCode code, TransactionId id, std::string message) const;

    [[nodiscard]]
    bool failpoint(CommitStage stage, TransactionContext & transaction, bool durable);

    void record_commit_duration(std::uint64_t duration_us) noexcept;

private:
    std::filesystem::path data_directory_;                       ///< 数据目录
    filesystem::FileSystem * filesystem_ {nullptr};              ///< 文件系统
    meta::MetaEngine * catalog_ {nullptr};                       ///< 元数据引擎
    storage::StorageEngine * storage_ {nullptr};                 ///< 存储引擎
    index::IndexEngine * index_engine_ {nullptr};                ///< 标量索引引擎
    vindex::VectorIndexEngine * vector_index_engine_ {nullptr};  ///< 向量索引引擎
    wal::WalStore * wal_ {nullptr};                              ///< WAL 存储
    TransactionId next_transaction_id_ {1};                      ///< 下一个事务 ID
    TransactionOptions options_;                                ///< 事务可选配置
    std::mutex writer_mutex_;                                   ///< 核心层单写者锁
    std::atomic_bool recovery_required_ {false};                 ///< 是否需要恢复
    std::atomic_uint64_t started_transactions_ {0};
    std::atomic_uint64_t committed_transactions_ {0};
    std::atomic_uint64_t aborted_transactions_ {0};
    std::atomic_uint64_t failed_commits_ {0};
    std::atomic_uint64_t total_commit_duration_us_ {0};
    std::atomic_uint64_t last_commit_duration_us_ {0};
    std::atomic_uint64_t maximum_commit_duration_us_ {0};
    std::atomic_uint64_t wal_size_bytes_ {0};
};

} // namespace litedb::core::transaction

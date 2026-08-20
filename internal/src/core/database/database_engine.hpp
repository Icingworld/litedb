#pragma once

#include <atomic>
#include <cstdint>
#include <expected>
#include <filesystem>
#include <memory>
#include <mutex>
#include <optional>
#include <string>

#include "core/catalog/catalog_publisher.hpp"
#include "core/catalog/catalog_store.hpp"
#include "core/database/database_manifest.hpp"
#include "core/executor/execution_error.hpp"
#include "core/executor/execution_result.hpp"
#include "core/filesystem/filesystem.hpp"
#include "core/index/index_engine.hpp"
#include "core/index/index_error.hpp"
#include "core/storage/schema_load_error.hpp"
#include "core/storage/storage_engine.hpp"
#include "core/transaction/transaction_manager.hpp"
#include "core/vindex/vector_index_engine.hpp"
#include "core/wal/wal_manager.hpp"

namespace litedb::core::physical_planner::plan
{

class PhysicalPlan;
class CreateCollectionPlan;
class CreateDatabasePlan;
class CreateIndexPlan;
class CreateVectorIndexPlan;
class DropCollectionPlan;
class DropDatabasePlan;
class DropIndexPlan;
class DropVectorIndexPlan;

} // namespace litedb::core::physical_planner::plan

namespace litedb::core::database
{

struct AutomaticCheckpointOptions
{
    /** 0 disables automatic checkpointing. */
    std::uint64_t wal_size_threshold_bytes {0};
};

/**
 * @brief 数据库配置
 */
struct DatabaseConfig
{
    std::filesystem::path data_dir; // 数据目录
    transaction::TransactionOptions transaction_options; // 事务测试与观测配置
    AutomaticCheckpointOptions automatic_checkpoint; // WAL size based checkpoint policy
    wal::WalDecodeLimits wal_decode_limits; // WAL 扫描与恢复资源预算
};

struct DatabaseObservability
{
    transaction::TransactionMetrics transaction;
    std::size_t recovered_committed_transactions {0};
    std::size_t replayed_writes {0};
    std::uint64_t automatic_checkpoint_attempts {0};
    std::uint64_t completed_automatic_checkpoints {0};
    std::uint64_t failed_automatic_checkpoints {0};
};

/**
 * @brief 数据库错误码
 */
enum class DatabaseErrorCode : std::uint8_t
{
    ManifestError = 0, // 数据库 manifest 错误
    CatalogError = 1, // catalog 引擎错误
    StorageError = 2, // 存储引擎错误
    IndexError = 3, // 索引引擎错误
    TransactionError = 5, // 事务初始化错误
};

/**
 * @brief 数据库错误
 */
using DatabaseError = error::Error;

class Session;

/**
 * @brief 数据库引擎
 */
class DatabaseEngine
{
public:
    DatabaseEngine(const DatabaseEngine &) = delete;

    DatabaseEngine & operator=(const DatabaseEngine &) = delete;

public:
    /**
     * @brief 打开数据库
     * @param config 配置
     * @return 结果
     */
    [[nodiscard]]
    static std::expected<std::unique_ptr<DatabaseEngine>, DatabaseError> open(
        DatabaseConfig config
    );

    /**
     * @brief 获取 catalog 引擎
     * @return catalog 引擎
     */
    [[nodiscard]]
    catalog::CatalogViewer catalog() const noexcept;

    /**
     * @brief 获取标量索引引擎
     * @return 标量索引引擎
     */
    [[nodiscard]]
    const index::IndexEngine & index_engine() const noexcept;

    [[nodiscard]]
    const vindex::VectorIndexEngine & vector_index_engine() const noexcept;

    [[nodiscard]]
    DatabaseObservability observability() const noexcept;

    /**
     * @brief 同步执行一次 checkpoint 并轮换 WAL
     */
    [[nodiscard]]
    std::expected<void, DatabaseError> checkpoint();

private:
    friend class Session;

    class PlanExecutionDispatcher;

    explicit DatabaseEngine(DatabaseConfig config);

    /**
     * @brief 初始化
     * @return 结果
     */
    [[nodiscard]]
    std::expected<void, DatabaseError> initialize();

    [[nodiscard]]
    std::expected<void, DatabaseError> cleanup_transaction_staging();

    /**
     * @brief 执行
     * @param plan 计划
     * @return 结果
     */
    [[nodiscard]]
    std::expected<executor::ExecutionResult, executor::ExecutionError> execute(
        const physical_planner::plan::PhysicalPlan & plan
    );

    void maybe_run_automatic_checkpoint();

    /**
     * @brief 执行创建数据库
     * @param plan 计划
     * @return 结果
     */
    [[nodiscard]]
    std::expected<executor::ExecutionResult, executor::ExecutionError> execute_create_database(
        const physical_planner::plan::CreateDatabasePlan & plan
    );

    /**
     * @brief 执行创建集合
     * @param plan 计划
     * @return 结果
     */
    [[nodiscard]]
    std::expected<executor::ExecutionResult, executor::ExecutionError> execute_create_collection(
        const physical_planner::plan::CreateCollectionPlan & plan
    );

    /**
     * @brief 执行创建索引
     * @param plan 计划
     * @return 结果
     */
    [[nodiscard]]
    std::expected<executor::ExecutionResult, executor::ExecutionError> execute_create_index(
        const physical_planner::plan::CreateIndexPlan & plan
    );

    /**
     * @brief 执行创建向量索引
     * @param plan 计划
     * @return 结果
     */
    [[nodiscard]]
    std::expected<executor::ExecutionResult, executor::ExecutionError> execute_create_vector_index(
        const physical_planner::plan::CreateVectorIndexPlan & plan
    );

    /**
     * @brief 执行删除数据库
     * @param plan 计划
     * @return 结果
     */
    [[nodiscard]]
    std::expected<executor::ExecutionResult, executor::ExecutionError> execute_drop_database(
        const physical_planner::plan::DropDatabasePlan & plan
    );

    /**
     * @brief 执行删除集合
     * @param plan 计划
     * @return 结果
     */
    [[nodiscard]]
    std::expected<executor::ExecutionResult, executor::ExecutionError> execute_drop_collection(
        const physical_planner::plan::DropCollectionPlan & plan
    );

    /**
     * @brief 执行删除索引
     * @param plan 计划
     * @return 结果
     */
    [[nodiscard]]
    std::expected<executor::ExecutionResult, executor::ExecutionError> execute_drop_index(
        const physical_planner::plan::DropIndexPlan & plan
    );

    /**
     * @brief 执行删除向量索引
     * @param plan 计划
     * @return 结果
     */
    [[nodiscard]]
    std::expected<executor::ExecutionResult, executor::ExecutionError> execute_drop_vector_index(
        const physical_planner::plan::DropVectorIndexPlan & plan
    );

    [[nodiscard]]
    std::expected<executor::ExecutionResult, executor::ExecutionError>
    commit_catalog_transaction(catalog::CatalogSnapshot snapshot, std::size_t affected_rows);

    /**
     * @brief 从 catalog 恢复存储
     * @return 结果
     */
    [[nodiscard]]
    std::expected<void, storage::StorageError> restore_storage_from_catalog();

    /**
     * @brief 从 catalog 错误创建执行错误
     * @param error catalog 错误
     * @return 执行错误
     */
    [[nodiscard]]
    static executor::ExecutionError from_catalog_error(catalog::CatalogError error);

    /**
     * @brief 从 schema 错误创建执行错误
     * @param error schema 错误
     * @return 执行错误
     */
    [[nodiscard]]
    static executor::ExecutionError from_schema_error(storage::SchemaLoadError error);

    /**
     * @brief 从存储错误创建执行错误
     * @param error 存储错误
     * @return 执行错误
     */
    [[nodiscard]]
    static executor::ExecutionError from_storage_error(storage::StorageError error);

    /**
     * @brief 从索引错误创建执行错误
     * @param error 索引错误
     * @return 执行错误
     */
    [[nodiscard]]
    static executor::ExecutionError from_index_error(index::IndexError error);

    [[nodiscard]]
    static executor::ExecutionError from_vector_index_error(vindex::VectorIndexError error);

private:
    std::filesystem::path data_directory_; // 数据目录
    filesystem::FileSystem filesystem_; // 文件系统
    DatabaseManifest manifest_; // 数据库 manifest
    catalog::CatalogPublisher catalog_; // 在线 Catalog 发布者
    storage::StorageEngine storage_; // 存储引擎
    index::IndexEngine index_engine_; // 索引引擎
    vindex::VectorIndexEngine vector_index_engine_; // 向量索引引擎
    std::optional<wal::WalManager> wal_manager_; // WAL 分段管理器
    std::unique_ptr<transaction::TransactionManager> transaction_manager_; // 事务管理器
    transaction::TransactionOptions transaction_options_; // 事务配置
    AutomaticCheckpointOptions automatic_checkpoint_; // WAL size based checkpoint policy
    wal::WalDecodeLimits wal_decode_limits_; // WAL 扫描与恢复资源预算
    std::size_t recovered_committed_transactions_ {0}; // 启动发现的已提交事务数
    std::size_t replayed_writes_ {0}; // 启动 redo 写入数
    std::atomic_uint64_t automatic_checkpoint_attempts_ {0};
    std::atomic_uint64_t completed_automatic_checkpoints_ {0};
    std::atomic_uint64_t failed_automatic_checkpoints_ {0};
    mutable std::mutex mutex_; // SQL、checkpoint 与观测的串行化边界
};

} // namespace litedb::core::database

namespace litedb::core::error
{
template <>
struct ErrorTraits<database::DatabaseErrorCode>
{
    static constexpr ErrorCategory category = ErrorCategory::Database;
};
} // namespace litedb::core::error

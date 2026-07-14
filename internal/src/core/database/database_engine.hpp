#pragma once

#include <expected>
#include <filesystem>
#include <memory>
#include <mutex>
#include <string>

#include "core/database/database_manifest.hpp"
#include "core/executor/execution_error.hpp"
#include "core/executor/execution_result.hpp"
#include "core/filesystem/filesystem.hpp"
#include "core/index/index_error.hpp"
#include "core/index/index_engine.hpp"
#include "core/meta/meta_engine.hpp"
#include "core/meta/meta_store.hpp"
#include "core/schema/schema_error.hpp"
#include "core/storage/storage_engine.hpp"

namespace litedb::core::physical_plan
{

class PhysicalStatementPlan;
class PhysicalCreateCollectionPlan;
class PhysicalCreateDatabasePlan;
class PhysicalCreateIndexPlan;
class PhysicalCreateVectorIndexPlan;
class PhysicalDropCollectionPlan;
class PhysicalDropDatabasePlan;
class PhysicalDropIndexPlan;
class PhysicalDropVectorIndexPlan;

} // namespace litedb::core::physical_plan

namespace litedb::core::database
{

/**
 * @brief 数据库配置
 */
struct DatabaseConfig
{
    std::filesystem::path data_dir;     ///< 数据目录
};

/**
 * @brief 数据库错误码
 */
enum class DatabaseErrorCode
{
    ManifestError,    ///< 数据库 manifest 错误
    MetaError,        ///< meta 引擎错误
    StorageError,     ///< 存储引擎错误
    IndexError,       ///< 索引管理器错误
};

/**
 * @brief 数据库错误
 */
struct DatabaseError
{
    DatabaseErrorCode code;    ///< 错误码
    std::string message;       ///< 错误消息
};

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
    static std::expected<std::unique_ptr<DatabaseEngine>, DatabaseError> open(DatabaseConfig config);

    /**
     * @brief 获取 meta 引擎
     * @return meta 引擎
     */
    [[nodiscard]]
    const meta::MetaEngine & meta() const noexcept;

    /**
     * @brief 获取索引管理器
     * @return 索引管理器
     */
    [[nodiscard]]
    const index::IndexEngine & index_engine() const noexcept;

private:
    friend class Session;

    explicit DatabaseEngine(DatabaseConfig config);

    /**
     * @brief 初始化
     * @return 结果
     */
    [[nodiscard]]
    std::expected<void, DatabaseError> initialize();

    /**
     * @brief 执行
     * @param plan 计划
     * @return 结果
     */
    [[nodiscard]]
    std::expected<executor::ExecutionResult, executor::ExecutionError> execute(
        const physical_plan::PhysicalStatementPlan & plan
    );

    /**
     * @brief 执行创建数据库
     * @param plan 计划
     * @return 结果
     */
    [[nodiscard]]
    std::expected<executor::ExecutionResult, executor::ExecutionError> execute_create_database(
        const physical_plan::PhysicalCreateDatabasePlan & plan
    );

    /**
     * @brief 执行创建集合
     * @param plan 计划
     * @return 结果
     */
    [[nodiscard]]
    std::expected<executor::ExecutionResult, executor::ExecutionError> execute_create_collection(
        const physical_plan::PhysicalCreateCollectionPlan & plan
    );

    /**
     * @brief 执行创建索引
     * @param plan 计划
     * @return 结果
     */
    [[nodiscard]]
    std::expected<executor::ExecutionResult, executor::ExecutionError> execute_create_index(
        const physical_plan::PhysicalCreateIndexPlan & plan
    );

    /**
     * @brief 执行创建向量索引
     * @param plan 计划
     * @return 结果
     */
    [[nodiscard]]
    std::expected<executor::ExecutionResult, executor::ExecutionError> execute_create_vector_index(
        const physical_plan::PhysicalCreateVectorIndexPlan & plan
    );

    /**
     * @brief 执行删除数据库
     * @param plan 计划
     * @return 结果
     */
    [[nodiscard]]
    std::expected<executor::ExecutionResult, executor::ExecutionError> execute_drop_database(
        const physical_plan::PhysicalDropDatabasePlan & plan
    );

    /**
     * @brief 执行删除集合
     * @param plan 计划
     * @return 结果
     */
    [[nodiscard]]
    std::expected<executor::ExecutionResult, executor::ExecutionError> execute_drop_collection(
        const physical_plan::PhysicalDropCollectionPlan & plan
    );

    /**
     * @brief 执行删除索引
     * @param plan 计划
     * @return 结果
     */
    [[nodiscard]]
    std::expected<executor::ExecutionResult, executor::ExecutionError> execute_drop_index(
        const physical_plan::PhysicalDropIndexPlan & plan
    );

    /**
     * @brief 执行删除向量索引
     * @param plan 计划
     * @return 结果
     */
    [[nodiscard]]
    std::expected<executor::ExecutionResult, executor::ExecutionError> execute_drop_vector_index(
        const physical_plan::PhysicalDropVectorIndexPlan & plan
    );

    /**
     * @brief 从 meta 恢复存储
     * @return 结果
     */
    [[nodiscard]]
    std::expected<void, storage::StorageError> restore_storage_from_meta();

    /**
     * @brief 从 meta 错误创建执行错误
     * @param error meta 错误
     * @param location 位置
     * @return 执行错误
     */
    [[nodiscard]]
    static executor::ExecutionError from_meta_error(
        meta::MetaEngineError error,
        parser::ast::AstNodeLocation location
    );

    /**
     * @brief 从 schema 错误创建执行错误
     * @param error schema 错误
     * @param location 位置
     * @return 执行错误
     */
    [[nodiscard]]
    static executor::ExecutionError from_schema_error(
        schema::SchemaError error,
        parser::ast::AstNodeLocation location
    );

    /**
     * @brief 从存储错误创建执行错误
     * @param error 存储错误
     * @param location 位置
     * @return 执行错误
     */
    [[nodiscard]]
    static executor::ExecutionError from_storage_error(
        storage::StorageError error,
        parser::ast::AstNodeLocation location
    );

    /**
     * @brief 从索引错误创建执行错误
     * @param error 索引错误
     * @param location 位置
     * @return 执行错误
     */
    [[nodiscard]]
    static executor::ExecutionError from_index_error(
        index::IndexError error,
        parser::ast::AstNodeLocation location
    );

private:
    filesystem::FileSystem filesystem_;    ///< 文件系统
    DatabaseManifest manifest_;            ///< 数据库 manifest
    meta::MetaStore meta_store_;           ///< meta 存储
    meta::MetaEngine meta_;                ///< meta 引擎
    storage::StorageEngine storage_;       ///< 存储引擎
    index::IndexEngine index_engine_;      ///< 索引引擎
    std::mutex mutex_;                     ///< 互斥锁
};

} // namespace litedb::core::database

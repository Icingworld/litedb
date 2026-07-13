#pragma once

#include <expected>

#include "core/meta/meta_engine.hpp"
#include "core/executor/execution_error.hpp"
#include "core/executor/execution_result.hpp"
#include "core/index/index_manager.hpp"
#include "core/physical_plan/statement/physical_statement_plan.hpp"
#include "core/storage/storage_engine.hpp"

namespace litedb::core::physical_plan
{

class PhysicalCreateCollectionPlan;
class PhysicalCreateDatabasePlan;
class PhysicalCreateIndexPlan;
class PhysicalCreateVectorIndexPlan;
class PhysicalDropCollectionPlan;
class PhysicalDropDatabasePlan;
class PhysicalDropIndexPlan;
class PhysicalDropVectorIndexPlan;

} // namespace litedb::core::physical_plan

namespace litedb::core::executor
{

/**
 * @brief DDL 变更处理器
 */
class DdlMutationHandler
{
public:
    virtual ~DdlMutationHandler() noexcept = default;

    virtual std::expected<ExecutionResult, ExecutionError> execute_create_database(
        const physical_plan::PhysicalCreateDatabasePlan & plan,
        meta::MetaEngine & catalog,
        storage::StorageEngine & storage,
        index::IndexManager & index_manager
    ) = 0;

    virtual std::expected<ExecutionResult, ExecutionError> execute_create_collection(
        const physical_plan::PhysicalCreateCollectionPlan & plan,
        meta::MetaEngine & catalog,
        storage::StorageEngine & storage,
        index::IndexManager & index_manager
    ) = 0;

    virtual std::expected<ExecutionResult, ExecutionError> execute_create_index(
        const physical_plan::PhysicalCreateIndexPlan & plan,
        meta::MetaEngine & catalog,
        storage::StorageEngine & storage,
        index::IndexManager & index_manager
    ) = 0;

    virtual std::expected<ExecutionResult, ExecutionError> execute_create_vector_index(
        const physical_plan::PhysicalCreateVectorIndexPlan & plan,
        meta::MetaEngine & catalog,
        storage::StorageEngine & storage,
        index::IndexManager & index_manager
    ) = 0;

    virtual std::expected<ExecutionResult, ExecutionError> execute_drop_database(
        const physical_plan::PhysicalDropDatabasePlan & plan,
        meta::MetaEngine & catalog,
        storage::StorageEngine & storage,
        index::IndexManager & index_manager
    ) = 0;

    virtual std::expected<ExecutionResult, ExecutionError> execute_drop_collection(
        const physical_plan::PhysicalDropCollectionPlan & plan,
        meta::MetaEngine & catalog,
        storage::StorageEngine & storage,
        index::IndexManager & index_manager
    ) = 0;

    virtual std::expected<ExecutionResult, ExecutionError> execute_drop_index(
        const physical_plan::PhysicalDropIndexPlan & plan,
        meta::MetaEngine & catalog,
        storage::StorageEngine & storage,
        index::IndexManager & index_manager
    ) = 0;

    virtual std::expected<ExecutionResult, ExecutionError> execute_drop_vector_index(
        const physical_plan::PhysicalDropVectorIndexPlan & plan,
        meta::MetaEngine & catalog,
        storage::StorageEngine & storage,
        index::IndexManager & index_manager
    ) = 0;
};

/**
 * @brief 执行器
 */
class Executor
{
public:
    Executor(
        meta::MetaEngine & catalog,
        storage::StorageEngine & storage,
        index::IndexManager & index_manager,
        DdlMutationHandler * ddl_handler = nullptr
    ) noexcept;

public:
    /**
     * @brief 执行语句计划
     * @param plan 语句计划
     * @return 执行结果
     */
    [[nodiscard]]
    std::expected<ExecutionResult, ExecutionError> execute(const physical_plan::PhysicalStatementPlan & plan);

private:
    meta::MetaEngine & catalog_;                        ///< 目录
    storage::StorageEngine & storage_;                  ///< 存储引擎
    index::IndexManager & index_manager_;               ///< 索引管理器
    DdlMutationHandler * ddl_handler_ {nullptr};        ///< DDL 变更处理器
};

} // namespace litedb::core::executor

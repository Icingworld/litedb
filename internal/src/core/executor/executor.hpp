#pragma once

#include <expected>

#include "core/catalog/catalog.hpp"
#include "core/executor/execution_error.hpp"
#include "core/executor/execution_result.hpp"
#include "core/index/index_manager.hpp"
#include "core/logical_plan/statement/statement_plan.hpp"
#include "core/storage/storage_manager.hpp"

namespace litedb::core::planner::plan
{

class CreateCollectionPlan;
class CreateDatabasePlan;
class CreateIndexPlan;
class CreateVectorIndexPlan;
class DropCollectionPlan;
class DropDatabasePlan;
class DropIndexPlan;
class DropVectorIndexPlan;

} // namespace litedb::core::planner::plan

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
        const planner::plan::CreateDatabasePlan & plan,
        catalog::Catalog & catalog,
        storage::StorageManager & storage,
        index::IndexManager & index_manager
    ) = 0;

    virtual std::expected<ExecutionResult, ExecutionError> execute_create_collection(
        const planner::plan::CreateCollectionPlan & plan,
        catalog::Catalog & catalog,
        storage::StorageManager & storage,
        index::IndexManager & index_manager
    ) = 0;

    virtual std::expected<ExecutionResult, ExecutionError> execute_create_index(
        const planner::plan::CreateIndexPlan & plan,
        catalog::Catalog & catalog,
        storage::StorageManager & storage,
        index::IndexManager & index_manager
    ) = 0;

    virtual std::expected<ExecutionResult, ExecutionError> execute_create_vector_index(
        const planner::plan::CreateVectorIndexPlan & plan,
        catalog::Catalog & catalog,
        storage::StorageManager & storage,
        index::IndexManager & index_manager
    ) = 0;

    virtual std::expected<ExecutionResult, ExecutionError> execute_drop_database(
        const planner::plan::DropDatabasePlan & plan,
        catalog::Catalog & catalog,
        storage::StorageManager & storage,
        index::IndexManager & index_manager
    ) = 0;

    virtual std::expected<ExecutionResult, ExecutionError> execute_drop_collection(
        const planner::plan::DropCollectionPlan & plan,
        catalog::Catalog & catalog,
        storage::StorageManager & storage,
        index::IndexManager & index_manager
    ) = 0;

    virtual std::expected<ExecutionResult, ExecutionError> execute_drop_index(
        const planner::plan::DropIndexPlan & plan,
        catalog::Catalog & catalog,
        storage::StorageManager & storage,
        index::IndexManager & index_manager
    ) = 0;

    virtual std::expected<ExecutionResult, ExecutionError> execute_drop_vector_index(
        const planner::plan::DropVectorIndexPlan & plan,
        catalog::Catalog & catalog,
        storage::StorageManager & storage,
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
        catalog::Catalog & catalog,
        storage::StorageManager & storage,
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
    std::expected<ExecutionResult, ExecutionError> execute(const planner::plan::StatementPlan & plan);

private:
    catalog::Catalog & catalog_;                        ///< 目录
    storage::StorageManager & storage_;                 ///< 存储管理器
    index::IndexManager & index_manager_;               ///< 索引管理器
    DdlMutationHandler * ddl_handler_ {nullptr};        ///< DDL 变更处理器
};

} // namespace litedb::core::executor

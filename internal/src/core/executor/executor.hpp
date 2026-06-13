#pragma once

#include <expected>

#include "core/catalog/catalog.hpp"
#include "core/executor/execution_error.hpp"
#include "core/executor/execution_result.hpp"
#include "core/index/index_manager.hpp"
#include "core/planner/statement/statement_plan.hpp"
#include "core/storage/storage_manager.hpp"

namespace litedb::core::planner
{

class CreateCollectionPlan;
class CreateDatabasePlan;
class CreateIndexPlan;
class DropCollectionPlan;
class DropDatabasePlan;
class DropIndexPlan;

} // namespace litedb::core::planner

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
        const planner::CreateDatabasePlan & plan,
        catalog::Catalog & catalog,
        storage::StorageManager & storage,
        index::IndexManager & index_manager
    ) = 0;

    virtual std::expected<ExecutionResult, ExecutionError> execute_create_collection(
        const planner::CreateCollectionPlan & plan,
        catalog::Catalog & catalog,
        storage::StorageManager & storage,
        index::IndexManager & index_manager
    ) = 0;

    virtual std::expected<ExecutionResult, ExecutionError> execute_create_index(
        const planner::CreateIndexPlan & plan,
        catalog::Catalog & catalog,
        storage::StorageManager & storage,
        index::IndexManager & index_manager
    ) = 0;

    virtual std::expected<ExecutionResult, ExecutionError> execute_drop_database(
        const planner::DropDatabasePlan & plan,
        catalog::Catalog & catalog,
        storage::StorageManager & storage,
        index::IndexManager & index_manager
    ) = 0;

    virtual std::expected<ExecutionResult, ExecutionError> execute_drop_collection(
        const planner::DropCollectionPlan & plan,
        catalog::Catalog & catalog,
        storage::StorageManager & storage,
        index::IndexManager & index_manager
    ) = 0;

    virtual std::expected<ExecutionResult, ExecutionError> execute_drop_index(
        const planner::DropIndexPlan & plan,
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
    std::expected<ExecutionResult, ExecutionError> execute(const planner::StatementPlan & plan);

private:
    catalog::Catalog & catalog_;                        ///< 目录
    storage::StorageManager & storage_;                 ///< 存储管理器
    index::IndexManager & index_manager_;               ///< 索引管理器
    DdlMutationHandler * ddl_handler_ {nullptr};        ///< DDL 变更处理器
};

} // namespace litedb::core::executor

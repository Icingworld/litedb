#pragma once

#include <expected>

#include "core/meta/meta_engine.hpp"
#include "core/executor/execution_error.hpp"
#include "core/executor/execution_result.hpp"
#include "core/index/index_engine.hpp"
#include "core/physical_plan/statement/physical_statement_plan.hpp"
#include "core/storage/storage_engine.hpp"
#include "core/transaction/transaction_manager.hpp"
#include "core/vindex/vector_index_engine.hpp"

namespace litedb::core::executor
{

/**
 * @brief 执行器
 */
class Executor
{
public:
    /**
     * @brief 构造只支持查询和非 DML 命令的执行器
     * @details DML 必须使用带 TransactionManager 的构造函数。
     */
    Executor(
        meta::MetaEngine & catalog,
        storage::StorageEngine & storage,
        index::IndexEngine & index_engine,
        vindex::VectorIndexEngine & vector_index_engine
    ) noexcept;

    Executor(
        meta::MetaEngine & catalog,
        storage::StorageEngine & storage,
        index::IndexEngine & index_engine,
        vindex::VectorIndexEngine & vector_index_engine,
        transaction::TransactionManager & transaction_manager
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
    index::IndexEngine & index_engine_;                  ///< 索引引擎
    vindex::VectorIndexEngine * vector_index_engine_ {nullptr};
    transaction::TransactionManager * transaction_manager_ {nullptr};
};

} // namespace litedb::core::executor

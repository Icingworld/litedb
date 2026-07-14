#pragma once

#include <expected>

#include "core/meta/meta_engine.hpp"
#include "core/executor/execution_error.hpp"
#include "core/executor/execution_result.hpp"
#include "core/index/index_manager.hpp"
#include "core/physical_plan/statement/physical_statement_plan.hpp"
#include "core/storage/storage_engine.hpp"

namespace litedb::core::executor
{

/**
 * @brief 执行器
 */
class Executor
{
public:
    Executor(
        meta::MetaEngine & catalog,
        storage::StorageEngine & storage,
        index::IndexManager & index_manager
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
};

} // namespace litedb::core::executor

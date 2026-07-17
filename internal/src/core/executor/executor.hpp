#pragma once

#include <expected>

#include "core/meta/meta_engine.hpp"
#include "core/executor/execution_error.hpp"
#include "core/executor/execution_result.hpp"
#include "core/index/index_engine.hpp"
#include "core/physical_plan/statement/physical_statement_plan.hpp"
#include "core/storage/storage_engine.hpp"
#include "core/vindex/vector_index_engine.hpp"

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
        index::IndexEngine & index_engine,
        vindex::VectorIndexEngine & vector_index_engine
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
};

} // namespace litedb::core::executor

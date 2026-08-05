#pragma once

#include <expected>

#include "core/meta/meta_engine.hpp"
#include "core/executor/execution_error.hpp"
#include "core/executor/execution_result.hpp"
#include "core/index/index_engine.hpp"
#include "core/physical_planner/plan/physical_plan.hpp"
#include "core/storage/storage_engine.hpp"
#include "core/transaction/transaction_manager.hpp"
#include "core/vindex/vector_index_engine.hpp"

namespace litedb::core::executor
{

class Executor
{
public:
    Executor(
        meta::CatalogView catalog,
        storage::StorageEngine & storage,
        index::IndexEngine & index_engine,
        vindex::VectorIndexEngine & vector_index_engine
    ) noexcept;

    Executor(
        meta::CatalogView catalog,
        storage::StorageEngine & storage,
        index::IndexEngine & index_engine,
        vindex::VectorIndexEngine & vector_index_engine,
        transaction::TransactionManager & transaction_manager
    ) noexcept;

    [[nodiscard]]
    std::expected<ExecutionResult, ExecutionError> execute(
        const physical_planner::plan::PhysicalPlan & plan
    );

private:
    meta::CatalogView catalog_;
    storage::StorageEngine & storage_;
    index::IndexEngine & index_engine_;
    vindex::VectorIndexEngine * vector_index_engine_ {nullptr};
    transaction::TransactionManager * transaction_manager_ {nullptr};
};

} // namespace litedb::core::executor

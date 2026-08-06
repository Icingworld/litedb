#pragma once

#include "core/index/index_engine.hpp"
#include "core/meta/meta_engine.hpp"
#include "core/storage/storage_engine.hpp"
#include "core/transaction/transaction_manager.hpp"
#include "core/vindex/vector_index_engine.hpp"

namespace litedb::core::executor
{

/**
 * @brief 执行阶段共享依赖
 *
 * The context deliberately contains all dependencies required by a production
 * Executor.  Individual workers may use only a subset, but the public
 * Executor contract no longer permits a partially initialized runtime.
 */
struct ExecutionContext final
{
    meta::CatalogView catalog;
    storage::StorageEngine & storage;
    index::IndexEngine & index_engine;
    vindex::VectorIndexEngine & vector_index_engine;
    transaction::TransactionManager & transaction_manager;
};

} // namespace litedb::core::executor

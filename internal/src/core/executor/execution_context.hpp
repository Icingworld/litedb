#pragma once

#include "core/index/index_engine.hpp"
#include "core/catalog/catalog_viewer.hpp"
#include "core/storage/storage_engine.hpp"
#include "core/transaction/transaction_manager.hpp"
#include "core/vindex/vector_index_engine.hpp"

namespace litedb::core::executor
{

// 执行器上下文
struct ExecutionContext final
{
    catalog::CatalogViewer catalog;
    storage::StorageEngine & storage;
    index::IndexEngine & index_engine;
    vindex::VectorIndexEngine & vector_index_engine;
    transaction::TransactionManager & transaction_manager;
};

} // namespace litedb::core::executor

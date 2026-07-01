#pragma once

#include <expected>
#include <filesystem>

#include "core/catalog/in_memory_catalog.hpp"
#include "core/executor/executor.hpp"
#include "core/index/index_error.hpp"
#include "core/index/index_manager.hpp"
#include "core/persistence/catalog_store.hpp"
#include "core/persistence/manifest_store.hpp"
#include "core/persistence/persistent_collection_storage.hpp"
#include "core/schema/schema_error.hpp"
#include "core/storage/storage_manager.hpp"

namespace litedb::core::persistence
{

class PersistenceController final : public executor::DdlMutationHandler
{
public:
    PersistenceController(
        std::filesystem::path data_dir,
        catalog::InMemoryCatalog & catalog,
        storage::StorageManager & storage,
        index::IndexManager & index_manager
    );

    [[nodiscard]]
    std::expected<void, storage::StorageError> initialize();

    std::expected<executor::ExecutionResult, executor::ExecutionError> execute_create_database(
        const planner::plan::CreateDatabasePlan & plan,
        catalog::Catalog & catalog,
        storage::StorageManager & storage,
        index::IndexManager & index_manager
    ) override;

    std::expected<executor::ExecutionResult, executor::ExecutionError> execute_create_collection(
        const planner::plan::CreateCollectionPlan & plan,
        catalog::Catalog & catalog,
        storage::StorageManager & storage,
        index::IndexManager & index_manager
    ) override;

    std::expected<executor::ExecutionResult, executor::ExecutionError> execute_create_index(
        const planner::plan::CreateIndexPlan & plan,
        catalog::Catalog & catalog,
        storage::StorageManager & storage,
        index::IndexManager & index_manager
    ) override;

    std::expected<executor::ExecutionResult, executor::ExecutionError> execute_create_vector_index(
        const planner::plan::CreateVectorIndexPlan & plan,
        catalog::Catalog & catalog,
        storage::StorageManager & storage,
        index::IndexManager & index_manager
    ) override;

    std::expected<executor::ExecutionResult, executor::ExecutionError> execute_drop_database(
        const planner::plan::DropDatabasePlan & plan,
        catalog::Catalog & catalog,
        storage::StorageManager & storage,
        index::IndexManager & index_manager
    ) override;

    std::expected<executor::ExecutionResult, executor::ExecutionError> execute_drop_collection(
        const planner::plan::DropCollectionPlan & plan,
        catalog::Catalog & catalog,
        storage::StorageManager & storage,
        index::IndexManager & index_manager
    ) override;

    std::expected<executor::ExecutionResult, executor::ExecutionError> execute_drop_index(
        const planner::plan::DropIndexPlan & plan,
        catalog::Catalog & catalog,
        storage::StorageManager & storage,
        index::IndexManager & index_manager
    ) override;

    std::expected<executor::ExecutionResult, executor::ExecutionError> execute_drop_vector_index(
        const planner::plan::DropVectorIndexPlan & plan,
        catalog::Catalog & catalog,
        storage::StorageManager & storage,
        index::IndexManager & index_manager
    ) override;

private:
    [[nodiscard]]
    std::filesystem::path row_log_path(common::CollectionId collection_id) const;

    [[nodiscard]]
    std::expected<std::unique_ptr<PersistentCollectionStorage>, storage::StorageError> make_collection_storage(
        const schema::CollectionSchema & collection_schema
    ) const;

    [[nodiscard]]
    std::expected<void, storage::StorageError> restore_storage_from_catalog();

    [[nodiscard]]
    executor::ExecutionError from_catalog_error(catalog::CatalogError error, parser::ast::AstNodeLocation location) const;

    [[nodiscard]]
    executor::ExecutionError from_schema_error(schema::SchemaError error, parser::ast::AstNodeLocation location) const;

    [[nodiscard]]
    executor::ExecutionError from_storage_error(storage::StorageError error, parser::ast::AstNodeLocation location) const;

    [[nodiscard]]
    executor::ExecutionError from_index_error(index::IndexError error, parser::ast::AstNodeLocation location) const;

private:
    ManifestStore manifest_;
    CatalogStore catalog_store_;
    catalog::InMemoryCatalog * catalog_;
    storage::StorageManager * storage_;
    index::IndexManager * index_manager_;
};

} // namespace litedb::core::persistence

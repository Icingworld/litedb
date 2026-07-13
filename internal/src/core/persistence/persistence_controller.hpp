#pragma once

#include <expected>
#include <filesystem>

#include "core/meta/meta_engine.hpp"
#include "core/meta/meta_store.hpp"
#include "core/executor/executor.hpp"
#include "core/filesystem/filesystem.hpp"
#include "core/index/index_error.hpp"
#include "core/index/index_manager.hpp"
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
        meta::MetaEngine & catalog,
        storage::StorageManager & storage,
        index::IndexManager & index_manager
    );

    [[nodiscard]]
    std::expected<void, storage::StorageError> initialize();

    std::expected<executor::ExecutionResult, executor::ExecutionError> execute_create_database(
        const physical_plan::PhysicalCreateDatabasePlan & plan,
        meta::MetaEngine & catalog,
        storage::StorageManager & storage,
        index::IndexManager & index_manager
    ) override;

    std::expected<executor::ExecutionResult, executor::ExecutionError> execute_create_collection(
        const physical_plan::PhysicalCreateCollectionPlan & plan,
        meta::MetaEngine & catalog,
        storage::StorageManager & storage,
        index::IndexManager & index_manager
    ) override;

    std::expected<executor::ExecutionResult, executor::ExecutionError> execute_create_index(
        const physical_plan::PhysicalCreateIndexPlan & plan,
        meta::MetaEngine & catalog,
        storage::StorageManager & storage,
        index::IndexManager & index_manager
    ) override;

    std::expected<executor::ExecutionResult, executor::ExecutionError> execute_create_vector_index(
        const physical_plan::PhysicalCreateVectorIndexPlan & plan,
        meta::MetaEngine & catalog,
        storage::StorageManager & storage,
        index::IndexManager & index_manager
    ) override;

    std::expected<executor::ExecutionResult, executor::ExecutionError> execute_drop_database(
        const physical_plan::PhysicalDropDatabasePlan & plan,
        meta::MetaEngine & catalog,
        storage::StorageManager & storage,
        index::IndexManager & index_manager
    ) override;

    std::expected<executor::ExecutionResult, executor::ExecutionError> execute_drop_collection(
        const physical_plan::PhysicalDropCollectionPlan & plan,
        meta::MetaEngine & catalog,
        storage::StorageManager & storage,
        index::IndexManager & index_manager
    ) override;

    std::expected<executor::ExecutionResult, executor::ExecutionError> execute_drop_index(
        const physical_plan::PhysicalDropIndexPlan & plan,
        meta::MetaEngine & catalog,
        storage::StorageManager & storage,
        index::IndexManager & index_manager
    ) override;

    std::expected<executor::ExecutionResult, executor::ExecutionError> execute_drop_vector_index(
        const physical_plan::PhysicalDropVectorIndexPlan & plan,
        meta::MetaEngine & catalog,
        storage::StorageManager & storage,
        index::IndexManager & index_manager
    ) override;

private:
    [[nodiscard]]
    std::filesystem::path row_log_path(common::CollectionId collection_id) const;

    [[nodiscard]]
    std::expected<std::unique_ptr<PersistentCollectionStorage>, storage::StorageError> make_collection_storage(
        const schema::CollectionSchema & collection_schema
    );

    [[nodiscard]]
    std::expected<void, storage::StorageError> restore_storage_from_meta();

    [[nodiscard]]
    executor::ExecutionError from_meta_error(meta::MetaEngineError error, parser::ast::AstNodeLocation location) const;

    [[nodiscard]]
    executor::ExecutionError from_schema_error(schema::SchemaError error, parser::ast::AstNodeLocation location) const;

    [[nodiscard]]
    executor::ExecutionError from_storage_error(storage::StorageError error, parser::ast::AstNodeLocation location) const;

    [[nodiscard]]
    executor::ExecutionError from_index_error(index::IndexError error, parser::ast::AstNodeLocation location) const;

private:
    filesystem::FileSystem filesystem_;
    ManifestStore manifest_;
    meta::MetaStore meta_store_;
    meta::MetaEngine * catalog_;
    storage::StorageManager * storage_;
    index::IndexManager * index_manager_;
};

} // namespace litedb::core::persistence

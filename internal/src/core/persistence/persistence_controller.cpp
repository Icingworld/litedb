#include "core/persistence/persistence_controller.hpp"

#include <memory>
#include <optional>
#include <string>
#include <utility>
#include <vector>

#include "core/planner/plan/command/create_collection_plan.hpp"
#include "core/planner/plan/command/create_database_plan.hpp"
#include "core/planner/plan/command/create_index_plan.hpp"
#include "core/planner/plan/command/create_vector_index_plan.hpp"
#include "core/planner/plan/command/drop_collection_plan.hpp"
#include "core/planner/plan/command/drop_database_plan.hpp"
#include "core/planner/plan/command/drop_index_plan.hpp"
#include "core/planner/plan/command/drop_vector_index_plan.hpp"
#include "core/persistence/persistent_collection_storage.hpp"
#include "core/schema/schema_loader.hpp"

namespace litedb::core::persistence
{

namespace
{

executor::ExecutionResult command_result(std::size_t affected_rows)
{
    executor::ExecutionResult result;
    result.kind = executor::ExecutionResultKind::Command;
    result.affected_rows = affected_rows;
    return result;
}

} // namespace

PersistenceController::PersistenceController(
    std::filesystem::path data_dir,
    catalog::InMemoryCatalog & catalog,
    storage::StorageManager & storage,
    index::IndexManager & index_manager
)
    : manifest_(std::move(data_dir))
    , catalog_store_(manifest_.catalog_path())
    , catalog_(&catalog)
    , storage_(&storage)
    , index_manager_(&index_manager)
{
}

std::expected<void, storage::StorageError> PersistenceController::initialize()
{
    auto initialized = manifest_.ensure_initialized();
    if (!initialized.has_value()) {
        return std::unexpected(std::move(initialized.error()));
    }

    auto snapshot = catalog_store_.load_or_empty();
    if (!snapshot.has_value()) {
        return std::unexpected(std::move(snapshot.error()));
    }

    auto restored = catalog_->restore(snapshot.value());
    if (!restored.has_value()) {
        return std::unexpected(storage::StorageError {
            .code = storage::StorageErrorCode::InvalidStorageFormat,
            .message = std::move(restored.error().message),
        });
    }

    auto saved = catalog_store_.save(catalog_->snapshot());
    if (!saved.has_value()) {
        return std::unexpected(std::move(saved.error()));
    }

    auto storage_restored = restore_storage_from_catalog();
    if (!storage_restored.has_value()) {
        return std::unexpected(std::move(storage_restored.error()));
    }

    auto indexes_rebuilt = index_manager_->rebuild_all(*catalog_, *storage_);
    if (!indexes_rebuilt.has_value()) {
        return std::unexpected(storage::StorageError {
            .code = storage::StorageErrorCode::InvalidStorageState,
            .message = std::move(indexes_rebuilt.error().message),
        });
    }

    return {};
}

std::expected<executor::ExecutionResult, executor::ExecutionError> PersistenceController::execute_create_database(
    const planner::plan::CreateDatabasePlan & plan,
    catalog::Catalog &,
    storage::StorageManager &,
    index::IndexManager &
)
{
    const auto existed = catalog_->find_database(plan.database_name()) != nullptr;

    catalog::InMemoryCatalog staged;
    auto restored = staged.restore(catalog_->snapshot());
    if (!restored.has_value()) {
        return std::unexpected(from_catalog_error(std::move(restored.error()), plan.location()));
    }

    auto created = staged.create_database(catalog::CreateDatabaseRequest {
        .name = plan.database_name(),
        .if_not_exists = plan.if_not_exists(),
    });
    if (!created.has_value()) {
        return std::unexpected(from_catalog_error(std::move(created.error()), plan.location()));
    }

    auto saved = catalog_store_.save(staged.snapshot());
    if (!saved.has_value()) {
        return std::unexpected(from_storage_error(std::move(saved.error()), plan.location()));
    }

    auto committed = catalog_->restore(staged.snapshot());
    if (!committed.has_value()) {
        return std::unexpected(from_catalog_error(std::move(committed.error()), plan.location()));
    }

    return command_result(existed ? 0 : 1);
}

std::expected<executor::ExecutionResult, executor::ExecutionError> PersistenceController::execute_create_collection(
    const planner::plan::CreateCollectionPlan & plan,
    catalog::Catalog &,
    storage::StorageManager &,
    index::IndexManager &
)
{
    const auto * existing = catalog_->find_collection(plan.database_id(), plan.collection_name());

    catalog::InMemoryCatalog staged;
    auto restored = staged.restore(catalog_->snapshot());
    if (!restored.has_value()) {
        return std::unexpected(from_catalog_error(std::move(restored.error()), plan.location()));
    }

    auto created = staged.create_collection(catalog::CreateCollectionRequest {
        .database_id = plan.database_id(),
        .name = plan.collection_name(),
        .if_not_exists = plan.if_not_exists(),
        .columns = plan.columns(),
        .comment = plan.comment(),
    });
    if (!created.has_value()) {
        return std::unexpected(from_catalog_error(std::move(created.error()), plan.location()));
    }

    const auto collection_id = created.value();
    if (existing != nullptr) {
        return command_result(0);
    }

    auto collection_schema = schema::load_collection_schema(staged, collection_id);
    if (!collection_schema.has_value()) {
        return std::unexpected(from_schema_error(std::move(collection_schema.error()), plan.location()));
    }

    auto collection_storage = make_collection_storage(collection_schema.value());
    if (!collection_storage.has_value()) {
        return std::unexpected(from_storage_error(std::move(collection_storage.error()), plan.location()));
    }

    auto saved = catalog_store_.save(staged.snapshot());
    if (!saved.has_value()) {
        return std::unexpected(from_storage_error(std::move(saved.error()), plan.location()));
    }

    auto committed = catalog_->restore(staged.snapshot());
    if (!committed.has_value()) {
        return std::unexpected(from_catalog_error(std::move(committed.error()), plan.location()));
    }

    if (storage_->find_collection(collection_id) == nullptr) {
        auto registered = storage_->register_collection(collection_id, std::move(collection_storage.value()));
        if (!registered.has_value()) {
            return std::unexpected(from_storage_error(std::move(registered.error()), plan.location()));
        }
    }

    return command_result(1);
}

std::expected<executor::ExecutionResult, executor::ExecutionError> PersistenceController::execute_create_index(
    const planner::plan::CreateIndexPlan & plan,
    catalog::Catalog &,
    storage::StorageManager &,
    index::IndexManager &
)
{
    const auto * existing = catalog_->find_index(plan.collection_id(), plan.index_name());

    catalog::InMemoryCatalog staged;
    auto restored = staged.restore(catalog_->snapshot());
    if (!restored.has_value()) {
        return std::unexpected(from_catalog_error(std::move(restored.error()), plan.location()));
    }

    auto created = staged.create_index(catalog::CreateIndexRequest {
        .collection_id = plan.collection_id(),
        .column_id = plan.column_id(),
        .name = plan.index_name(),
        .index_kind = plan.index_kind(),
        .unique = plan.unique(),
        .if_not_exists = plan.if_not_exists(),
    });
    if (!created.has_value()) {
        return std::unexpected(from_catalog_error(std::move(created.error()), plan.location()));
    }

    if (existing != nullptr) {
        return command_result(0);
    }

    const auto * index_entry = staged.find_index(created.value());
    if (index_entry == nullptr) {
        return std::unexpected(executor::ExecutionError {
            .code = executor::ExecutionErrorCode::CatalogError,
            .location = plan.location(),
            .message = "Created index metadata not found",
        });
    }

    auto collection_schema = schema::load_collection_schema(staged, plan.collection_id());
    if (!collection_schema.has_value()) {
        return std::unexpected(from_schema_error(std::move(collection_schema.error()), plan.location()));
    }

    auto * collection_storage = storage_->find_collection(plan.collection_id());
    if (collection_storage == nullptr) {
        return std::unexpected(executor::ExecutionError {
            .code = executor::ExecutionErrorCode::CollectionStorageNotFound,
            .location = plan.location(),
            .message = "Collection storage not found",
        });
    }

    index::IndexManager rebuilt_indexes;
    auto rebuilt = rebuilt_indexes.rebuild_all(staged, *storage_);
    if (!rebuilt.has_value()) {
        return std::unexpected(from_index_error(std::move(rebuilt.error()), plan.location()));
    }

    auto saved = catalog_store_.save(staged.snapshot());
    if (!saved.has_value()) {
        return std::unexpected(from_storage_error(std::move(saved.error()), plan.location()));
    }

    auto committed = catalog_->restore(staged.snapshot());
    if (!committed.has_value()) {
        return std::unexpected(from_catalog_error(std::move(committed.error()), plan.location()));
    }

    *index_manager_ = std::move(rebuilt_indexes);

    return command_result(1);
}

std::expected<executor::ExecutionResult, executor::ExecutionError> PersistenceController::execute_create_vector_index(
    const planner::plan::CreateVectorIndexPlan & plan,
    catalog::Catalog &,
    storage::StorageManager &,
    index::IndexManager &
)
{
    const auto * existing = catalog_->find_vector_index(plan.collection_id(), plan.index_name());

    catalog::InMemoryCatalog staged;
    auto restored = staged.restore(catalog_->snapshot());
    if (!restored.has_value()) {
        return std::unexpected(from_catalog_error(std::move(restored.error()), plan.location()));
    }

    auto created = staged.create_vector_index(catalog::CreateVectorIndexRequest {
        .collection_id = plan.collection_id(),
        .column_id = plan.column_id(),
        .name = plan.index_name(),
        .index_kind = plan.index_kind(),
        .metric = plan.metric(),
        .max_neighbors = plan.max_neighbors(),
        .ef_construction = plan.ef_construction(),
        .ef_search_default = plan.ef_search_default(),
        .random_seed = plan.random_seed(),
        .if_not_exists = plan.if_not_exists(),
    });
    if (!created.has_value()) {
        return std::unexpected(from_catalog_error(std::move(created.error()), plan.location()));
    }

    if (existing != nullptr) {
        return command_result(0);
    }

    auto saved = catalog_store_.save(staged.snapshot());
    if (!saved.has_value()) {
        return std::unexpected(from_storage_error(std::move(saved.error()), plan.location()));
    }

    auto committed = catalog_->restore(staged.snapshot());
    if (!committed.has_value()) {
        return std::unexpected(from_catalog_error(std::move(committed.error()), plan.location()));
    }

    return command_result(1);
}

std::expected<executor::ExecutionResult, executor::ExecutionError> PersistenceController::execute_drop_database(
    const planner::plan::DropDatabasePlan & plan,
    catalog::Catalog &,
    storage::StorageManager &,
    index::IndexManager &
)
{
    if (!plan.database_id().has_value()) {
        return command_result(0);
    }

    const auto collections = catalog_->list_collections(plan.database_id().value());
    std::vector<common::CollectionId> collection_ids;
    collection_ids.reserve(collections.size());
    for (const auto * collection : collections) {
        if (collection != nullptr) {
            collection_ids.push_back(collection->id());
        }
    }

    catalog::InMemoryCatalog staged;
    auto restored = staged.restore(catalog_->snapshot());
    if (!restored.has_value()) {
        return std::unexpected(from_catalog_error(std::move(restored.error()), plan.location()));
    }

    auto dropped = staged.drop_database(catalog::DropDatabaseRequest {
        .name = plan.database_name(),
        .if_exists = plan.if_exists(),
    });
    if (!dropped.has_value()) {
        return std::unexpected(from_catalog_error(std::move(dropped.error()), plan.location()));
    }

    auto saved = catalog_store_.save(staged.snapshot());
    if (!saved.has_value()) {
        return std::unexpected(from_storage_error(std::move(saved.error()), plan.location()));
    }

    for (const auto collection_id : collection_ids) {
        (void) RowLog {row_log_path(collection_id), collection_id}.mark_dropped();
    }

    auto committed = catalog_->restore(staged.snapshot());
    if (!committed.has_value()) {
        return std::unexpected(from_catalog_error(std::move(committed.error()), plan.location()));
    }

    for (const auto collection_id : collection_ids) {
        if (storage_->find_collection(collection_id) != nullptr) {
            (void) storage_->drop_collection(collection_id);
        }
        index_manager_->drop_collection_indexes(collection_id);
    }

    return command_result(1);
}

std::expected<executor::ExecutionResult, executor::ExecutionError> PersistenceController::execute_drop_collection(
    const planner::plan::DropCollectionPlan & plan,
    catalog::Catalog &,
    storage::StorageManager &,
    index::IndexManager &
)
{
    if (!plan.collection_id().has_value()) {
        return command_result(0);
    }

    catalog::InMemoryCatalog staged;
    auto restored = staged.restore(catalog_->snapshot());
    if (!restored.has_value()) {
        return std::unexpected(from_catalog_error(std::move(restored.error()), plan.location()));
    }

    auto dropped = staged.drop_collection(catalog::DropCollectionRequest {
        .database_id = plan.database_id(),
        .name = plan.collection_name(),
        .if_exists = plan.if_exists(),
    });
    if (!dropped.has_value()) {
        return std::unexpected(from_catalog_error(std::move(dropped.error()), plan.location()));
    }

    auto saved = catalog_store_.save(staged.snapshot());
    if (!saved.has_value()) {
        return std::unexpected(from_storage_error(std::move(saved.error()), plan.location()));
    }

    const auto collection_id = plan.collection_id().value();
    (void) RowLog {row_log_path(collection_id), collection_id}.mark_dropped();

    auto committed = catalog_->restore(staged.snapshot());
    if (!committed.has_value()) {
        return std::unexpected(from_catalog_error(std::move(committed.error()), plan.location()));
    }

    if (storage_->find_collection(collection_id) != nullptr) {
        (void) storage_->drop_collection(collection_id);
    }
    index_manager_->drop_collection_indexes(collection_id);

    return command_result(1);
}

std::expected<executor::ExecutionResult, executor::ExecutionError> PersistenceController::execute_drop_index(
    const planner::plan::DropIndexPlan & plan,
    catalog::Catalog &,
    storage::StorageManager &,
    index::IndexManager &
)
{
    const auto * existing = catalog_->find_index(plan.collection_id(), plan.index_name());
    if (existing == nullptr && plan.if_exists()) {
        return command_result(0);
    }
    const auto index_id = existing != nullptr ? std::optional<common::IndexId> {existing->id()} : std::nullopt;

    catalog::InMemoryCatalog staged;
    auto restored = staged.restore(catalog_->snapshot());
    if (!restored.has_value()) {
        return std::unexpected(from_catalog_error(std::move(restored.error()), plan.location()));
    }

    auto dropped = staged.drop_index(catalog::DropIndexRequest {
        .collection_id = plan.collection_id(),
        .name = plan.index_name(),
        .if_exists = plan.if_exists(),
    });
    if (!dropped.has_value()) {
        return std::unexpected(from_catalog_error(std::move(dropped.error()), plan.location()));
    }

    auto saved = catalog_store_.save(staged.snapshot());
    if (!saved.has_value()) {
        return std::unexpected(from_storage_error(std::move(saved.error()), plan.location()));
    }

    auto committed = catalog_->restore(staged.snapshot());
    if (!committed.has_value()) {
        return std::unexpected(from_catalog_error(std::move(committed.error()), plan.location()));
    }

    if (index_id.has_value()) {
        auto dropped_index = index_manager_->drop_index(index_id.value());
        if (!dropped_index.has_value() && dropped_index.error().code != index::IndexErrorCode::IndexNotFound) {
            return std::unexpected(from_index_error(std::move(dropped_index.error()), plan.location()));
        }
    }

    return command_result(existing == nullptr ? 0 : 1);
}

std::expected<executor::ExecutionResult, executor::ExecutionError> PersistenceController::execute_drop_vector_index(
    const planner::plan::DropVectorIndexPlan & plan,
    catalog::Catalog &,
    storage::StorageManager &,
    index::IndexManager &
)
{
    const auto * existing = catalog_->find_vector_index(plan.collection_id(), plan.index_name());
    if (existing == nullptr && plan.if_exists()) {
        return command_result(0);
    }

    catalog::InMemoryCatalog staged;
    auto restored = staged.restore(catalog_->snapshot());
    if (!restored.has_value()) {
        return std::unexpected(from_catalog_error(std::move(restored.error()), plan.location()));
    }

    auto dropped = staged.drop_vector_index(catalog::DropVectorIndexRequest {
        .collection_id = plan.collection_id(),
        .name = plan.index_name(),
        .if_exists = plan.if_exists(),
    });
    if (!dropped.has_value()) {
        return std::unexpected(from_catalog_error(std::move(dropped.error()), plan.location()));
    }

    auto saved = catalog_store_.save(staged.snapshot());
    if (!saved.has_value()) {
        return std::unexpected(from_storage_error(std::move(saved.error()), plan.location()));
    }

    auto committed = catalog_->restore(staged.snapshot());
    if (!committed.has_value()) {
        return std::unexpected(from_catalog_error(std::move(committed.error()), plan.location()));
    }

    return command_result(existing == nullptr ? 0 : 1);
}

std::filesystem::path PersistenceController::row_log_path(common::CollectionId collection_id) const
{
    return manifest_.collections_dir() / (std::to_string(collection_id) + ".rows");
}

std::expected<std::unique_ptr<PersistentCollectionStorage>, storage::StorageError>
PersistenceController::make_collection_storage(const schema::CollectionSchema & collection_schema) const
{
    return PersistentCollectionStorage::open(
        collection_schema,
        RowLog {row_log_path(collection_schema.collection_id()), collection_schema.collection_id()}
    );
}

std::expected<void, storage::StorageError> PersistenceController::restore_storage_from_catalog()
{
    storage_->clear();
    for (const auto * database : catalog_->list_databases()) {
        if (database == nullptr) {
            continue;
        }
        for (const auto * collection : catalog_->list_collections(database->id())) {
            if (collection == nullptr) {
                continue;
            }

            auto collection_schema = schema::load_collection_schema(*catalog_, collection->id());
            if (!collection_schema.has_value()) {
                return std::unexpected(storage::StorageError {
                    .code = storage::StorageErrorCode::InvalidStorageFormat,
                    .message = std::move(collection_schema.error().message),
                });
            }

            auto collection_storage = make_collection_storage(collection_schema.value());
            if (!collection_storage.has_value()) {
                return std::unexpected(std::move(collection_storage.error()));
            }

            auto registered = storage_->register_collection(collection->id(), std::move(collection_storage.value()));
            if (!registered.has_value()) {
                return std::unexpected(std::move(registered.error()));
            }
        }
    }
    return {};
}

executor::ExecutionError PersistenceController::from_catalog_error(
    catalog::CatalogError error,
    parser::ast::AstNodeLocation location
) const
{
    return executor::ExecutionError {
        .code = executor::ExecutionErrorCode::CatalogError,
        .location = location,
        .message = std::move(error.message),
    };
}

executor::ExecutionError PersistenceController::from_schema_error(
    schema::SchemaError error,
    parser::ast::AstNodeLocation location
) const
{
    return executor::ExecutionError {
        .code = executor::ExecutionErrorCode::SchemaError,
        .location = location,
        .message = std::move(error.message),
    };
}

executor::ExecutionError PersistenceController::from_storage_error(
    storage::StorageError error,
    parser::ast::AstNodeLocation location
) const
{
    return executor::ExecutionError {
        .code = executor::ExecutionErrorCode::StorageError,
        .location = location,
        .message = std::move(error.message),
    };
}

executor::ExecutionError PersistenceController::from_index_error(
    index::IndexError error,
    parser::ast::AstNodeLocation location
) const
{
    return executor::ExecutionError {
        .code = executor::ExecutionErrorCode::IndexError,
        .location = location,
        .message = std::move(error.message),
    };
}

} // namespace litedb::core::persistence

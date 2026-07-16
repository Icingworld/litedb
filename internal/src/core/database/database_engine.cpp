#include "core/database/database_engine.hpp"

#include <memory>
#include <optional>
#include <string>
#include <utility>
#include <vector>

#include "core/filesystem/platform_filesystem.hpp"
#include "core/executor/executor.hpp"
#include "core/physical_plan/statement/physical_command_plan.hpp"
#include "core/schema/schema_loader.hpp"

namespace litedb::core::database
{

namespace
{

/**
 * @brief 从 manifest 错误创建数据库错误
 * @param error manifest 错误
 * @return 数据库错误
 */
DatabaseError to_database_error(ManifestError error)
{
    return DatabaseError {
        .code = DatabaseErrorCode::ManifestError,
        .message = std::move(error.message),
    };
}

/**
 * @brief 从 meta 引擎错误创建数据库错误
 * @param error meta 引擎错误
 * @return 数据库错误
 */
DatabaseError to_database_error(meta::MetaEngineError error)
{
    return DatabaseError {
        .code = DatabaseErrorCode::MetaError,
        .message = std::move(error.message),
    };
}

/**
 * @brief 从存储引擎错误创建数据库错误
 * @param error 存储引擎错误
 * @return 数据库错误
 */
DatabaseError to_database_error(storage::StorageError error)
{
    return DatabaseError {
        .code = DatabaseErrorCode::StorageError,
        .message = std::move(error.message),
    };
}

/**
 * @brief 从索引管理器错误创建数据库错误
 * @param error 索引管理器错误
 * @return 数据库错误
 */
DatabaseError to_database_error(index::IndexError error)
{
    return DatabaseError {
        .code = DatabaseErrorCode::IndexError,
        .message = std::move(error.message),
    };
}

/**
 * @brief 创建命令执行结果
 * @param affected_rows 受影响的行数
 * @return 命令执行结果
 */
executor::ExecutionResult command_result(std::size_t affected_rows)
{
    executor::ExecutionResult result;
    result.kind = executor::ExecutionResultKind::Command;
    result.affected_rows = affected_rows;
    return result;
}

} // namespace

DatabaseEngine::DatabaseEngine(DatabaseConfig config)
    : filesystem_(filesystem::create_platform_filesystem())
    , manifest_(config.data_dir, filesystem_)
    , meta_store_(manifest_.meta_path(), filesystem_)
    , meta_(meta_store_)
    , storage_(config.data_dir, filesystem_)
    , index_engine_(std::move(config.data_dir), filesystem_)
{
}

std::expected<std::unique_ptr<DatabaseEngine>, DatabaseError> DatabaseEngine::open(DatabaseConfig config)
{
    auto engine = std::unique_ptr<DatabaseEngine> {new DatabaseEngine(std::move(config))};
    auto initialized = engine->initialize();
    if (!initialized.has_value()) {
        return std::unexpected(std::move(initialized.error()));
    }
    return engine;
}

const meta::MetaEngine & DatabaseEngine::meta() const noexcept
{
    return meta_;
}

const index::IndexEngine & DatabaseEngine::index_engine() const noexcept
{
    return index_engine_;
}

std::expected<void, DatabaseError> DatabaseEngine::initialize()
{
    auto initialized = manifest_.ensure_initialized();
    if (!initialized.has_value()) {
        return std::unexpected(to_database_error(std::move(initialized.error())));
    }

    auto loaded = meta_.load();
    if (!loaded.has_value()) {
        return std::unexpected(to_database_error(std::move(loaded.error())));
    }

    auto initialized_meta = meta_.commit(meta_.snapshot());
    if (!initialized_meta.has_value()) {
        return std::unexpected(to_database_error(std::move(initialized_meta.error())));
    }

    auto storage_restored = restore_storage_from_meta();
    if (!storage_restored.has_value()) {
        return std::unexpected(to_database_error(std::move(storage_restored.error())));
    }

    auto indexes_restored = index_engine_.restore_all(meta_, storage_);
    if (!indexes_restored.has_value()) {
        return std::unexpected(to_database_error(std::move(indexes_restored.error())));
    }

    return {};
}

std::expected<executor::ExecutionResult, executor::ExecutionError> DatabaseEngine::execute(
    const physical_plan::PhysicalStatementPlan & plan
)
{
    using physical_plan::PhysicalStatementPlanKind;

    switch (plan.kind()) {
    case PhysicalStatementPlanKind::CreateDatabase:
        return execute_create_database(static_cast<const physical_plan::PhysicalCreateDatabasePlan &>(plan));
    case PhysicalStatementPlanKind::CreateCollection:
        return execute_create_collection(static_cast<const physical_plan::PhysicalCreateCollectionPlan &>(plan));
    case PhysicalStatementPlanKind::CreateIndex:
        return execute_create_index(static_cast<const physical_plan::PhysicalCreateIndexPlan &>(plan));
    case PhysicalStatementPlanKind::CreateVectorIndex:
        return execute_create_vector_index(static_cast<const physical_plan::PhysicalCreateVectorIndexPlan &>(plan));
    case PhysicalStatementPlanKind::DropDatabase:
        return execute_drop_database(static_cast<const physical_plan::PhysicalDropDatabasePlan &>(plan));
    case PhysicalStatementPlanKind::DropCollection:
        return execute_drop_collection(static_cast<const physical_plan::PhysicalDropCollectionPlan &>(plan));
    case PhysicalStatementPlanKind::DropIndex:
        return execute_drop_index(static_cast<const physical_plan::PhysicalDropIndexPlan &>(plan));
    case PhysicalStatementPlanKind::DropVectorIndex:
        return execute_drop_vector_index(static_cast<const physical_plan::PhysicalDropVectorIndexPlan &>(plan));
    default:
        executor::Executor executor {meta_, storage_, index_engine_};
        return executor.execute(plan);
    }
}

std::expected<executor::ExecutionResult, executor::ExecutionError> DatabaseEngine::execute_create_database(
    const physical_plan::PhysicalCreateDatabasePlan & plan
)
{
    const auto existed = meta_.find_database(plan.database_name()) != nullptr;

    meta::MetaEngine staged;
    auto restored = staged.restore(meta_.snapshot());
    if (!restored.has_value()) {
        return std::unexpected(from_meta_error(std::move(restored.error()), plan.location()));
    }

    auto created = staged.create_database(meta::CreateDatabaseRequest {
        .name = plan.database_name(),
        .if_not_exists = plan.if_not_exists(),
    });
    if (!created.has_value()) {
        return std::unexpected(from_meta_error(std::move(created.error()), plan.location()));
    }

    auto committed = meta_.commit(staged.snapshot());
    if (!committed.has_value()) {
        return std::unexpected(from_meta_error(std::move(committed.error()), plan.location()));
    }

    return command_result(existed ? 0 : 1);
}

std::expected<executor::ExecutionResult, executor::ExecutionError> DatabaseEngine::execute_create_collection(
    const physical_plan::PhysicalCreateCollectionPlan & plan
)
{
    const auto * existing = meta_.find_collection(plan.database_id(), plan.collection_name());

    meta::MetaEngine staged;
    auto restored = staged.restore(meta_.snapshot());
    if (!restored.has_value()) {
        return std::unexpected(from_meta_error(std::move(restored.error()), plan.location()));
    }

    auto created = staged.create_collection(meta::CreateCollectionRequest {
        .database_id = plan.database_id(),
        .name = plan.collection_name(),
        .if_not_exists = plan.if_not_exists(),
        .columns = plan.columns(),
        .comment = plan.comment(),
    });
    if (!created.has_value()) {
        return std::unexpected(from_meta_error(std::move(created.error()), plan.location()));
    }

    const auto collection_id = created.value();
    if (existing != nullptr) {
        return command_result(0);
    }

    auto collection_schema = schema::load_collection_schema(staged, collection_id);
    if (!collection_schema.has_value()) {
        return std::unexpected(from_schema_error(std::move(collection_schema.error()), plan.location()));
    }

    auto committed = meta_.commit(staged.snapshot());
    if (!committed.has_value()) {
        return std::unexpected(from_meta_error(std::move(committed.error()), plan.location()));
    }

    auto storage_created = storage_.create_collection(std::move(collection_schema.value()));
    if (!storage_created.has_value()) {
        return std::unexpected(from_storage_error(std::move(storage_created.error()), plan.location()));
    }

    return command_result(1);
}

std::expected<executor::ExecutionResult, executor::ExecutionError> DatabaseEngine::execute_create_index(
    const physical_plan::PhysicalCreateIndexPlan & plan
)
{
    const auto * existing = meta_.find_index(plan.collection_id(), plan.index_name());

    meta::MetaEngine staged;
    auto restored = staged.restore(meta_.snapshot());
    if (!restored.has_value()) {
        return std::unexpected(from_meta_error(std::move(restored.error()), plan.location()));
    }

    auto created = staged.create_index(meta::CreateIndexRequest {
        .collection_id = plan.collection_id(),
        .column_ids = {plan.column_id()},
        .name = plan.index_name(),
        .kind = plan.index_kind(),
        .unique = plan.unique(),
        .if_not_exists = plan.if_not_exists(),
    });
    if (!created.has_value()) {
        return std::unexpected(from_meta_error(std::move(created.error()), plan.location()));
    }

    if (existing != nullptr) {
        return command_result(0);
    }

    const auto * index_entry = staged.find_index(created.value());
    if (index_entry == nullptr) {
        return std::unexpected(executor::ExecutionError {
            .code = executor::ExecutionErrorCode::MetaError,
            .location = plan.location(),
            .message = "Created index metadata not found",
        });
    }

    auto collection_schema = schema::load_collection_schema(staged, plan.collection_id());
    if (!collection_schema.has_value()) {
        return std::unexpected(from_schema_error(std::move(collection_schema.error()), plan.location()));
    }

    if (!storage_.contains_collection(plan.collection_id())) {
        return std::unexpected(executor::ExecutionError {
            .code = executor::ExecutionErrorCode::CollectionNotFound,
            .location = plan.location(),
            .message = "Collection storage not found",
        });
    }

    auto runtime_created = index_engine_.create_index(*index_entry, collection_schema.value(), storage_);
    if (!runtime_created.has_value()) {
        return std::unexpected(from_index_error(std::move(runtime_created.error()), plan.location()));
    }

    auto committed = meta_.commit(staged.snapshot());
    if (!committed.has_value()) {
        (void) index_engine_.drop_index(index_entry->id());
        return std::unexpected(from_meta_error(std::move(committed.error()), plan.location()));
    }

    return command_result(1);
}

std::expected<executor::ExecutionResult, executor::ExecutionError> DatabaseEngine::execute_create_vector_index(
    const physical_plan::PhysicalCreateVectorIndexPlan & plan
)
{
    const auto * existing = meta_.find_vector_index(plan.collection_id(), plan.index_name());

    meta::MetaEngine staged;
    auto restored = staged.restore(meta_.snapshot());
    if (!restored.has_value()) {
        return std::unexpected(from_meta_error(std::move(restored.error()), plan.location()));
    }

    auto created = staged.create_vector_index(meta::CreateVectorIndexRequest {
        .collection_id = plan.collection_id(),
        .column_id = plan.column_id(),
        .name = plan.index_name(),
        .kind = plan.index_kind(),
        .metric = plan.metric(),
        .hnsw_options = {
            .max_neighbors = plan.max_neighbors(),
            .ef_construction = plan.ef_construction(),
            .ef_search_default = plan.ef_search_default(),
            .random_seed = plan.random_seed(),
        },
        .if_not_exists = plan.if_not_exists(),
    });
    if (!created.has_value()) {
        return std::unexpected(from_meta_error(std::move(created.error()), plan.location()));
    }

    if (existing != nullptr) {
        return command_result(0);
    }

    auto committed = meta_.commit(staged.snapshot());
    if (!committed.has_value()) {
        return std::unexpected(from_meta_error(std::move(committed.error()), plan.location()));
    }

    return command_result(1);
}

std::expected<executor::ExecutionResult, executor::ExecutionError> DatabaseEngine::execute_drop_database(
    const physical_plan::PhysicalDropDatabasePlan & plan
)
{
    if (!plan.database_id().has_value()) {
        return command_result(0);
    }

    const auto collections = meta_.list_collections(plan.database_id().value());
    std::vector<common::CollectionId> collection_ids;
    collection_ids.reserve(collections.size());
    for (const auto * collection : collections) {
        if (collection != nullptr) {
            collection_ids.push_back(collection->id());
        }
    }

    meta::MetaEngine staged;
    auto restored = staged.restore(meta_.snapshot());
    if (!restored.has_value()) {
        return std::unexpected(from_meta_error(std::move(restored.error()), plan.location()));
    }

    auto dropped = staged.drop_database(meta::DropDatabaseRequest {
        .name = plan.database_name(),
        .if_exists = plan.if_exists(),
    });
    if (!dropped.has_value()) {
        return std::unexpected(from_meta_error(std::move(dropped.error()), plan.location()));
    }

    auto committed = meta_.commit(staged.snapshot());
    if (!committed.has_value()) {
        return std::unexpected(from_meta_error(std::move(committed.error()), plan.location()));
    }

    for (const auto collection_id : collection_ids) {
        if (storage_.contains_collection(collection_id)) {
            (void) storage_.drop_collection(collection_id);
        }
        auto indexes_dropped = index_engine_.drop_collection_indexes(collection_id);
        if (!indexes_dropped.has_value()) {
            return std::unexpected(from_index_error(std::move(indexes_dropped.error()), plan.location()));
        }
    }

    return command_result(1);
}

std::expected<executor::ExecutionResult, executor::ExecutionError> DatabaseEngine::execute_drop_collection(
    const physical_plan::PhysicalDropCollectionPlan & plan
)
{
    if (!plan.collection_id().has_value()) {
        return command_result(0);
    }

    meta::MetaEngine staged;
    auto restored = staged.restore(meta_.snapshot());
    if (!restored.has_value()) {
        return std::unexpected(from_meta_error(std::move(restored.error()), plan.location()));
    }

    auto dropped = staged.drop_collection(meta::DropCollectionRequest {
        .database_id = plan.database_id(),
        .name = plan.collection_name(),
        .if_exists = plan.if_exists(),
    });
    if (!dropped.has_value()) {
        return std::unexpected(from_meta_error(std::move(dropped.error()), plan.location()));
    }

    const auto collection_id = plan.collection_id().value();
    auto committed = meta_.commit(staged.snapshot());
    if (!committed.has_value()) {
        return std::unexpected(from_meta_error(std::move(committed.error()), plan.location()));
    }

    if (storage_.contains_collection(collection_id)) {
        (void) storage_.drop_collection(collection_id);
    }
    auto indexes_dropped = index_engine_.drop_collection_indexes(collection_id);
    if (!indexes_dropped.has_value()) {
        return std::unexpected(from_index_error(std::move(indexes_dropped.error()), plan.location()));
    }

    return command_result(1);
}

std::expected<executor::ExecutionResult, executor::ExecutionError> DatabaseEngine::execute_drop_index(
    const physical_plan::PhysicalDropIndexPlan & plan
)
{
    const auto * existing = meta_.find_index(plan.collection_id(), plan.index_name());
    if (existing == nullptr && plan.if_exists()) {
        return command_result(0);
    }
    const auto index_id = existing != nullptr ? std::optional<common::IndexId> {existing->id()} : std::nullopt;

    meta::MetaEngine staged;
    auto restored = staged.restore(meta_.snapshot());
    if (!restored.has_value()) {
        return std::unexpected(from_meta_error(std::move(restored.error()), plan.location()));
    }

    auto dropped = staged.drop_index(meta::DropIndexRequest {
        .collection_id = plan.collection_id(),
        .name = plan.index_name(),
        .if_exists = plan.if_exists(),
    });
    if (!dropped.has_value()) {
        return std::unexpected(from_meta_error(std::move(dropped.error()), plan.location()));
    }

    auto committed = meta_.commit(staged.snapshot());
    if (!committed.has_value()) {
        return std::unexpected(from_meta_error(std::move(committed.error()), plan.location()));
    }

    if (index_id.has_value()) {
        auto dropped_index = index_engine_.drop_index(index_id.value());
        if (!dropped_index.has_value() && dropped_index.error().code != index::IndexErrorCode::IndexNotFound) {
            return std::unexpected(from_index_error(std::move(dropped_index.error()), plan.location()));
        }
    }

    return command_result(existing == nullptr ? 0 : 1);
}

std::expected<executor::ExecutionResult, executor::ExecutionError> DatabaseEngine::execute_drop_vector_index(
    const physical_plan::PhysicalDropVectorIndexPlan & plan
)
{
    const auto * existing = meta_.find_vector_index(plan.collection_id(), plan.index_name());
    if (existing == nullptr && plan.if_exists()) {
        return command_result(0);
    }

    meta::MetaEngine staged;
    auto restored = staged.restore(meta_.snapshot());
    if (!restored.has_value()) {
        return std::unexpected(from_meta_error(std::move(restored.error()), plan.location()));
    }

    auto dropped = staged.drop_vector_index(meta::DropVectorIndexRequest {
        .collection_id = plan.collection_id(),
        .name = plan.index_name(),
        .if_exists = plan.if_exists(),
    });
    if (!dropped.has_value()) {
        return std::unexpected(from_meta_error(std::move(dropped.error()), plan.location()));
    }

    auto committed = meta_.commit(staged.snapshot());
    if (!committed.has_value()) {
        return std::unexpected(from_meta_error(std::move(committed.error()), plan.location()));
    }

    return command_result(existing == nullptr ? 0 : 1);
}

std::expected<void, storage::StorageError> DatabaseEngine::restore_storage_from_meta()
{
    storage_.clear();
    for (const auto * database : meta_.list_databases()) {
        if (database == nullptr) {
            continue;
        }
        for (const auto * collection : meta_.list_collections(database->id())) {
            if (collection == nullptr) {
                continue;
            }

            auto collection_schema = schema::load_collection_schema(meta_, collection->id());
            if (!collection_schema.has_value()) {
                return std::unexpected(storage::StorageError {
                    .code = storage::StorageErrorCode::StoreError,
                    .message = std::move(collection_schema.error().message),
                    .storage_store_code = storage::StorageStoreErrorCode::InvalidFormat,
                });
            }

            auto opened = storage_.open_collection(std::move(collection_schema.value()));
            if (!opened.has_value()) {
                return std::unexpected(std::move(opened.error()));
            }
        }
    }
    return {};
}

executor::ExecutionError DatabaseEngine::from_meta_error(
    meta::MetaEngineError error,
    parser::ast::AstNodeLocation location
)
{
    return executor::ExecutionError {
        .code = executor::ExecutionErrorCode::MetaError,
        .location = location,
        .message = std::move(error.message),
    };
}

executor::ExecutionError DatabaseEngine::from_schema_error(
    schema::SchemaError error,
    parser::ast::AstNodeLocation location
)
{
    return executor::ExecutionError {
        .code = executor::ExecutionErrorCode::SchemaError,
        .location = location,
        .message = std::move(error.message),
    };
}

executor::ExecutionError DatabaseEngine::from_storage_error(
    storage::StorageError error,
    parser::ast::AstNodeLocation location
)
{
    return executor::ExecutionError {
        .code = executor::ExecutionErrorCode::StorageError,
        .location = location,
        .message = std::move(error.message),
    };
}

executor::ExecutionError DatabaseEngine::from_index_error(
    index::IndexError error,
    parser::ast::AstNodeLocation location
)
{
    return executor::ExecutionError {
        .code = executor::ExecutionErrorCode::IndexError,
        .location = location,
        .message = std::move(error.message),
    };
}

} // namespace litedb::core::database

#include "core/database/database_engine.hpp"

#include <memory>
#include <optional>
#include <string>
#include <system_error>
#include <utility>
#include <vector>

#include "core/filesystem/platform_filesystem.hpp"
#include "core/executor/executor.hpp"
#include "core/physical_plan/statement/physical_command_plan.hpp"
#include "core/schema/schema_loader.hpp"
#include "core/wal/recovery_manager.hpp"

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
 * @brief 从索引引擎错误创建数据库错误
 * @param error 索引引擎错误
 * @return 数据库错误
 */
DatabaseError to_database_error(index::IndexError error)
{
    return DatabaseError {
        .code = DatabaseErrorCode::IndexError,
        .message = std::move(error.message),
    };
}

DatabaseError to_database_error(vindex::VectorIndexError error)
{
    return DatabaseError {
        .code = DatabaseErrorCode::IndexError,
        .message = std::move(error.message),
    };
}

DatabaseError to_database_error(wal::WalError error)
{
    return DatabaseError {
        .code = DatabaseErrorCode::WalError,
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

bool writes_wal(physical_plan::PhysicalStatementPlanKind kind) noexcept
{
    using physical_plan::PhysicalStatementPlanKind;
    switch (kind) {
    case PhysicalStatementPlanKind::CreateDatabase:
    case PhysicalStatementPlanKind::CreateCollection:
    case PhysicalStatementPlanKind::CreateIndex:
    case PhysicalStatementPlanKind::CreateVectorIndex:
    case PhysicalStatementPlanKind::DropDatabase:
    case PhysicalStatementPlanKind::DropCollection:
    case PhysicalStatementPlanKind::DropIndex:
    case PhysicalStatementPlanKind::DropVectorIndex:
    case PhysicalStatementPlanKind::Insert:
    case PhysicalStatementPlanKind::Update:
    case PhysicalStatementPlanKind::Delete:
        return true;
    case PhysicalStatementPlanKind::Use:
    case PhysicalStatementPlanKind::ShowDatabases:
    case PhysicalStatementPlanKind::ShowCollections:
    case PhysicalStatementPlanKind::ShowIndexes:
    case PhysicalStatementPlanKind::ShowVectorIndexes:
    case PhysicalStatementPlanKind::DescribeCollection:
    case PhysicalStatementPlanKind::Query:
        return false;
    }
    return false;
}

} // namespace

DatabaseEngine::DatabaseEngine(DatabaseConfig config)
    : data_directory_(std::move(config.data_dir))
    , filesystem_(filesystem::create_platform_filesystem())
    , manifest_(data_directory_, filesystem_)
    , meta_store_(manifest_.meta_path(), filesystem_)
    , meta_(meta_store_)
    , storage_(data_directory_, filesystem_)
    , index_engine_(data_directory_, filesystem_)
    , vector_index_engine_(data_directory_ / "vindexes", filesystem_)
    , transaction_options_(std::move(config.transaction_options))
    , automatic_checkpoint_(config.automatic_checkpoint)
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

const vindex::VectorIndexEngine & DatabaseEngine::vector_index_engine() const noexcept
{
    return vector_index_engine_;
}

executor::ExecutionError from_transaction_error(
    transaction::TransactionError error,
    parser::ast::AstNodeLocation location
)
{
    return executor::ExecutionError {
        .code = executor::ExecutionErrorCode::TransactionError,
        .location = location,
        .message = "Transaction " + std::to_string(error.transaction_id) + ": " + std::move(error.message),
    };
}

DatabaseObservability DatabaseEngine::observability() const noexcept
{
    return DatabaseObservability {
        .transaction = transaction_manager_ != nullptr
                           ? transaction_manager_->metrics()
                           : transaction::TransactionMetrics {},
        .recovered_committed_transactions = recovered_committed_transactions_,
        .replayed_writes = replayed_writes_,
        .automatic_checkpoint_attempts = automatic_checkpoint_attempts_.load(std::memory_order_relaxed),
        .completed_automatic_checkpoints = completed_automatic_checkpoints_.load(std::memory_order_relaxed),
        .failed_automatic_checkpoints = failed_automatic_checkpoints_.load(std::memory_order_relaxed),
    };
}

void DatabaseEngine::maybe_run_automatic_checkpoint()
{
    if (transaction_manager_ == nullptr || automatic_checkpoint_.wal_size_threshold_bytes == 0) return;
    if (transaction_manager_->metrics().wal_size_bytes < automatic_checkpoint_.wal_size_threshold_bytes) return;

    automatic_checkpoint_attempts_.fetch_add(1, std::memory_order_relaxed);
    auto checkpointed = transaction_manager_->checkpoint();
    if (checkpointed) {
        completed_automatic_checkpoints_.fetch_add(1, std::memory_order_relaxed);
    } else {
        // The triggering statement is already durably committed. Expose a
        // maintenance failure through metrics instead of returning a failure
        // that could make the caller retry the committed statement.
        failed_automatic_checkpoints_.fetch_add(1, std::memory_order_relaxed);
    }
}

std::expected<void, DatabaseError> DatabaseEngine::checkpoint()
{
    if (transaction_manager_ == nullptr) {
        return std::unexpected(DatabaseError {
            .code = DatabaseErrorCode::TransactionError,
            .message = "Database transaction manager is not initialized",
        });
    }
    auto checkpointed = transaction_manager_->checkpoint();
    if (!checkpointed) {
        return std::unexpected(DatabaseError {
            .code = checkpointed.error().code == transaction::TransactionErrorCode::WalError
                        ? DatabaseErrorCode::WalError
                        : DatabaseErrorCode::TransactionError,
            .message = std::move(checkpointed.error().message),
        });
    }
    return {};
}

std::expected<void, DatabaseError> DatabaseEngine::cleanup_transaction_staging()
{
    std::error_code error;
    std::filesystem::remove_all(data_directory_ / ".transactions", error);
    if (error) {
        return std::unexpected(DatabaseError {
            .code = DatabaseErrorCode::TransactionError,
            .message = "Failed to clean stale transaction staging: " + error.message(),
        });
    }
    return {};
}

std::expected<void, DatabaseError> DatabaseEngine::initialize()
{
    auto cleaned = cleanup_transaction_staging();
    if (!cleaned) {
        return std::unexpected(std::move(cleaned.error()));
    }

    auto initialized = manifest_.ensure_initialized();
    if (!initialized.has_value()) {
        return std::unexpected(to_database_error(std::move(initialized.error())));
    }

    auto opened_wal = wal::WalManager::open(data_directory_ / "wal", filesystem_);
    if (!opened_wal) {
        return std::unexpected(to_database_error(std::move(opened_wal.error())));
    }
    wal_manager_ = std::move(*opened_wal);

    auto recovered = wal::RecoveryManager::recover(data_directory_, filesystem_, *wal_manager_);
    if (!recovered) {
        return std::unexpected(to_database_error(std::move(recovered.error())));
    }
    recovered_committed_transactions_ = recovered->committed_transactions;
    replayed_writes_ = recovered->replayed_writes;

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

    auto vector_indexes_restored = vector_index_engine_.restore_all(meta_, storage_);
    if (!vector_indexes_restored.has_value()) {
        return std::unexpected(to_database_error(std::move(vector_indexes_restored.error())));
    }

    transaction_manager_ = std::make_unique<transaction::TransactionManager>(
        data_directory_, filesystem_, meta_, storage_, index_engine_, vector_index_engine_, *wal_manager_,
        recovered->maximum_transaction_id, std::move(transaction_options_)
    );

    return {};
}

std::expected<executor::ExecutionResult, executor::ExecutionError> DatabaseEngine::execute(
    const physical_plan::PhysicalStatementPlan & plan
)
{
    using physical_plan::PhysicalStatementPlanKind;

    if (transaction_manager_ != nullptr && transaction_manager_->recovery_required()) {
        return std::unexpected(executor::ExecutionError {
            .code = executor::ExecutionErrorCode::TransactionError,
            .location = plan.location(),
            .message = "Database requires WAL recovery before accepting more requests",
        });
    }

    auto executed = [&]() -> std::expected<executor::ExecutionResult, executor::ExecutionError> {
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
            executor::Executor executor {meta_, storage_, index_engine_, vector_index_engine_, *transaction_manager_};
            return executor.execute(plan);
        }
    }();
    if (executed && writes_wal(plan.kind())) {
        maybe_run_automatic_checkpoint();
    }
    return executed;
}

std::expected<executor::ExecutionResult, executor::ExecutionError> DatabaseEngine::commit_catalog_transaction(
    meta::MetaSnapshot snapshot,
    std::size_t affected_rows,
    parser::ast::AstNodeLocation location
)
{
    auto transaction = transaction_manager_->begin_implicit();
    if (!transaction) return std::unexpected(from_transaction_error(std::move(transaction.error()), location));
    auto staged = transaction_manager_->stage_catalog(*transaction, std::move(snapshot));
    if (!staged) {
        (void) transaction_manager_->abort(*transaction);
        return std::unexpected(from_transaction_error(std::move(staged.error()), location));
    }
    auto committed = transaction_manager_->commit(*transaction);
    if (!committed) return std::unexpected(from_transaction_error(std::move(committed.error()), location));
    return command_result(affected_rows);
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
    if (existed) return command_result(0);
    return commit_catalog_transaction(staged.snapshot(), 1, plan.location());
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

    return commit_catalog_transaction(staged.snapshot(), 1, plan.location());
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

    return commit_catalog_transaction(staged.snapshot(), 1, plan.location());
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

    return commit_catalog_transaction(staged.snapshot(), 1, plan.location());
}

std::expected<executor::ExecutionResult, executor::ExecutionError> DatabaseEngine::execute_drop_database(
    const physical_plan::PhysicalDropDatabasePlan & plan
)
{
    if (!plan.database_id().has_value()) {
        return command_result(0);
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

    return commit_catalog_transaction(staged.snapshot(), 1, plan.location());
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

    return commit_catalog_transaction(staged.snapshot(), 1, plan.location());
}

std::expected<executor::ExecutionResult, executor::ExecutionError> DatabaseEngine::execute_drop_index(
    const physical_plan::PhysicalDropIndexPlan & plan
)
{
    const auto * existing = meta_.find_index(plan.collection_id(), plan.index_name());
    if (existing == nullptr && plan.if_exists()) {
        return command_result(0);
    }
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

    return commit_catalog_transaction(staged.snapshot(), existing == nullptr ? 0 : 1, plan.location());
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

    return commit_catalog_transaction(staged.snapshot(), existing == nullptr ? 0 : 1, plan.location());
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

executor::ExecutionError DatabaseEngine::from_vector_index_error(
    vindex::VectorIndexError error,
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

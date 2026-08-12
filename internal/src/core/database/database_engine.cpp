#include "core/database/database_engine.hpp"

#include <memory>
#include <optional>
#include <string>
#include <system_error>
#include <utility>
#include <vector>

#include "core/catalog/catalog_editor.hpp"
#include "core/filesystem/platform_filesystem.hpp"
#include "core/executor/executor.hpp"
#include "core/physical_planner/plan/dispatcher/physical_plan_dispatcher.hpp"
#include "core/physical_planner/plan/command/create_collection_plan.hpp"
#include "core/physical_planner/plan/command/create_database_plan.hpp"
#include "core/physical_planner/plan/command/create_index_plan.hpp"
#include "core/physical_planner/plan/command/create_vector_index_plan.hpp"
#include "core/physical_planner/plan/command/describe_collection_plan.hpp"
#include "core/physical_planner/plan/command/drop_collection_plan.hpp"
#include "core/physical_planner/plan/command/drop_database_plan.hpp"
#include "core/physical_planner/plan/command/drop_index_plan.hpp"
#include "core/physical_planner/plan/command/drop_vector_index_plan.hpp"
#include "core/physical_planner/plan/command/show_collections_plan.hpp"
#include "core/physical_planner/plan/command/show_databases_plan.hpp"
#include "core/physical_planner/plan/command/show_indexes_plan.hpp"
#include "core/physical_planner/plan/command/show_vector_indexes_plan.hpp"
#include "core/physical_planner/plan/command/use_plan.hpp"
#include "core/physical_planner/plan/physical_plan.hpp"
#include "core/storage/schema_loader.hpp"
#include "core/wal/recovery_manager.hpp"

namespace litedb::core::database
{

namespace
{

/**
 * @brief 浠?manifest 閿欒鍒涘缓鏁版嵁搴撻敊璇?
 * @param error manifest 閿欒
 * @return 鏁版嵁搴撻敊璇?
 */
DatabaseError wrap_database_error(DatabaseErrorCode code, error::Error error)
{
    auto message = error.message();
    return DatabaseError {code, message, std::move(error)};
}

DatabaseError manifest_error_to_database(ManifestError error)
{
    return wrap_database_error(DatabaseErrorCode::ManifestError, std::move(error));
}

/**
 * @brief 浠?catalog 寮曟搸閿欒鍒涘缓鏁版嵁搴撻敊璇?
 * @param error catalog 寮曟搸閿欒
 * @return 鏁版嵁搴撻敊璇?
 */
DatabaseError catalog_error_to_database(catalog::CatalogError error)
{
    return wrap_database_error(DatabaseErrorCode::CatalogError, std::move(error));
}

/**
 * @brief 浠庡瓨鍌ㄥ紩鎿庨敊璇垱寤烘暟鎹簱閿欒
 * @param error 瀛樺偍寮曟搸閿欒
 * @return 鏁版嵁搴撻敊璇?
 */
DatabaseError storage_error_to_database(storage::StorageError error)
{
    return wrap_database_error(DatabaseErrorCode::StorageError, std::move(error));
}

/**
 * @brief 浠庣储寮曞紩鎿庨敊璇垱寤烘暟鎹簱閿欒
 * @param error 绱㈠紩寮曟搸閿欒
 * @return 鏁版嵁搴撻敊璇?
 */
DatabaseError index_error_to_database(index::IndexError error)
{
    return wrap_database_error(DatabaseErrorCode::IndexError, std::move(error));
}

DatabaseError vector_error_to_database(vindex::VectorIndexError error)
{
    return wrap_database_error(DatabaseErrorCode::IndexError, std::move(error));
}

DatabaseError wal_error_to_database(wal::WalError error)
{
    return wrap_database_error(DatabaseErrorCode::WalError, std::move(error));
}

/**
 * @brief 鍒涘缓鍛戒护鎵ц缁撴灉
 * @param affected_rows 鍙楀奖鍝嶇殑琛屾暟
 * @return 鍛戒护鎵ц缁撴灉
 */
executor::ExecutionResult command_result(std::size_t affected_rows)
{
    executor::ExecutionResult result;
    result.kind = executor::ExecutionResultKind::Command;
    result.affected_rows = affected_rows;
    return result;
}

bool writes_wal(physical_planner::plan::PhysicalPlanKind kind) noexcept
{
    using physical_planner::plan::PhysicalPlanKind;
    switch (kind) {
    case PhysicalPlanKind::CreateDatabase:
    case PhysicalPlanKind::CreateCollection:
    case PhysicalPlanKind::CreateIndex:
    case PhysicalPlanKind::CreateVectorIndex:
    case PhysicalPlanKind::DropDatabase:
    case PhysicalPlanKind::DropCollection:
    case PhysicalPlanKind::DropIndex:
    case PhysicalPlanKind::DropVectorIndex:
    case PhysicalPlanKind::Insert:
    case PhysicalPlanKind::Update:
    case PhysicalPlanKind::Delete:
        return true;
    case PhysicalPlanKind::Use:
    case PhysicalPlanKind::ShowDatabases:
    case PhysicalPlanKind::ShowCollections:
    case PhysicalPlanKind::ShowIndexes:
    case PhysicalPlanKind::ShowVectorIndexes:
    case PhysicalPlanKind::DescribeCollection:
    case PhysicalPlanKind::Query:
        return false;
    }
    return false;
}

} // namespace

class DatabaseEngine::PlanExecutionDispatcher final
    : private physical_planner::plan::ConstPhysicalPlanDispatcher<
          DatabaseEngine::PlanExecutionDispatcher,
          std::expected<executor::ExecutionResult, executor::ExecutionError>
      >
{
    using Result = std::expected<executor::ExecutionResult, executor::ExecutionError>;
    using Dispatcher = physical_planner::plan::ConstPhysicalPlanDispatcher<
        PlanExecutionDispatcher,
        Result
    >;

    friend Dispatcher;

public:
    PlanExecutionDispatcher(DatabaseEngine & database, executor::Executor & executor) noexcept
        : database_(database)
        , executor_(executor)
    {
    }

    [[nodiscard]]
    Result dispatch(const physical_planner::plan::PhysicalPlan & plan)
    {
        return dispatch_plan(plan);
    }

private:
    Result visit_use_plan(const physical_planner::plan::UsePlan & plan)
    {
        return executor_.execute(plan);
    }

    Result visit_create_database_plan(const physical_planner::plan::CreateDatabasePlan & plan)
    {
        return database_.execute_create_database(plan);
    }

    Result visit_create_collection_plan(const physical_planner::plan::CreateCollectionPlan & plan)
    {
        return database_.execute_create_collection(plan);
    }

    Result visit_create_index_plan(const physical_planner::plan::CreateIndexPlan & plan)
    {
        return database_.execute_create_index(plan);
    }

    Result visit_create_vector_index_plan(const physical_planner::plan::CreateVectorIndexPlan & plan)
    {
        return database_.execute_create_vector_index(plan);
    }

    Result visit_drop_database_plan(const physical_planner::plan::DropDatabasePlan & plan)
    {
        return database_.execute_drop_database(plan);
    }

    Result visit_drop_collection_plan(const physical_planner::plan::DropCollectionPlan & plan)
    {
        return database_.execute_drop_collection(plan);
    }

    Result visit_drop_index_plan(const physical_planner::plan::DropIndexPlan & plan)
    {
        return database_.execute_drop_index(plan);
    }

    Result visit_drop_vector_index_plan(const physical_planner::plan::DropVectorIndexPlan & plan)
    {
        return database_.execute_drop_vector_index(plan);
    }

    Result visit_show_databases_plan(const physical_planner::plan::ShowDatabasesPlan & plan)
    {
        return executor_.execute(plan);
    }

    Result visit_show_collections_plan(const physical_planner::plan::ShowCollectionsPlan & plan)
    {
        return executor_.execute(plan);
    }

    Result visit_show_indexes_plan(const physical_planner::plan::ShowIndexesPlan & plan)
    {
        return executor_.execute(plan);
    }

    Result visit_show_vector_indexes_plan(const physical_planner::plan::ShowVectorIndexesPlan & plan)
    {
        return executor_.execute(plan);
    }

    Result visit_describe_collection_plan(const physical_planner::plan::DescribeCollectionPlan & plan)
    {
        return executor_.execute(plan);
    }

    Result visit_insert_plan(const physical_planner::plan::InsertPlan & plan)
    {
        return executor_.execute(plan);
    }

    Result visit_update_plan(const physical_planner::plan::UpdatePlan & plan)
    {
        return executor_.execute(plan);
    }

    Result visit_delete_plan(const physical_planner::plan::DeletePlan & plan)
    {
        return executor_.execute(plan);
    }

    Result visit_query_plan(const physical_planner::plan::QueryPlan & plan)
    {
        return executor_.execute(plan);
    }

private:
    DatabaseEngine & database_;
    executor::Executor & executor_;
};

DatabaseEngine::DatabaseEngine(DatabaseConfig config)
    : data_directory_(std::move(config.data_dir))
    , filesystem_(filesystem::create_platform_filesystem())
    , manifest_(data_directory_, filesystem_)
    , catalog_(manifest_.catalog_path(), filesystem_)
    , storage_(data_directory_, filesystem_)
    , index_engine_(data_directory_, filesystem_)
    , vector_index_engine_(data_directory_ / "vindexes", filesystem_)
    , transaction_options_(std::move(config.transaction_options))
    , automatic_checkpoint_(config.automatic_checkpoint)
    , wal_decode_limits_(config.wal_decode_limits)
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

catalog::CatalogViewer DatabaseEngine::catalog() const noexcept
{
    return catalog_.view();
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
    transaction::TransactionError error
)
{
    const auto * context = error.context<transaction::TransactionErrorContext>();
    auto message =
        "Transaction " +
        std::to_string(context != nullptr
                           ? context->transaction_id
                           : transaction::InvalidTransactionId) +
        ": " + error.message();
    return executor::ExecutionError {
        executor::ExecutionErrorCode::TransactionError,
        message,

        std::move(error),
    };
}

DatabaseObservability DatabaseEngine::observability() const noexcept
{
    std::scoped_lock lock {mutex_};
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
    std::scoped_lock lock {mutex_};
    if (transaction_manager_ == nullptr) {
        return std::unexpected(DatabaseError {
            DatabaseErrorCode::TransactionError,
            "Database transaction manager is not initialized",
        });
    }
    auto checkpointed = transaction_manager_->checkpoint();
    if (!checkpointed) {
        const auto code = checkpointed.error().is(transaction::TransactionErrorCode::WalError)
            ? DatabaseErrorCode::WalError
            : DatabaseErrorCode::TransactionError;
        auto message = checkpointed.error().message();
        return std::unexpected(DatabaseError {code, message, std::move(checkpointed.error())});
    }
    return {};
}

std::expected<void, DatabaseError> DatabaseEngine::cleanup_transaction_staging()
{
    std::error_code error;
    std::filesystem::remove_all(data_directory_ / ".transactions", error);
    if (error) {
        return std::unexpected(DatabaseError {
            DatabaseErrorCode::TransactionError,
            "Failed to clean stale transaction staging: " + error.message(),
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
        return std::unexpected(manifest_error_to_database(std::move(initialized.error())));
    }

    auto opened_wal = wal::WalManager::open(
        data_directory_ / "wal",
        filesystem_,
        wal_decode_limits_
    );
    if (!opened_wal) {
        return std::unexpected(wal_error_to_database(std::move(opened_wal.error())));
    }
    wal_manager_ = std::move(*opened_wal);

    auto recovered = wal::RecoveryManager::recover(
        data_directory_,
        filesystem_,
        *wal_manager_,
        wal_decode_limits_
    );
    if (!recovered) {
        return std::unexpected(wal_error_to_database(std::move(recovered.error())));
    }
    recovered_committed_transactions_ = recovered->committed_transactions;
    replayed_writes_ = recovered->replayed_writes;

    auto loaded = catalog_.open_or_initialize();
    if (!loaded.has_value()) {
        return std::unexpected(catalog_error_to_database(std::move(loaded.error())));
    }

    auto storage_restored = restore_storage_from_catalog();
    if (!storage_restored.has_value()) {
        return std::unexpected(storage_error_to_database(std::move(storage_restored.error())));
    }

    auto indexes_restored = index_engine_.restore_all(catalog(), storage_);
    if (!indexes_restored.has_value()) {
        return std::unexpected(index_error_to_database(std::move(indexes_restored.error())));
    }

    auto vector_indexes_restored = vector_index_engine_.restore_all(catalog(), storage_);
    if (!vector_indexes_restored.has_value()) {
        return std::unexpected(vector_error_to_database(std::move(vector_indexes_restored.error())));
    }

    transaction_manager_ = std::make_unique<transaction::TransactionManager>(
        data_directory_, filesystem_, catalog_, storage_, index_engine_, vector_index_engine_, *wal_manager_,
        recovered->maximum_transaction_id, std::move(transaction_options_)
    );

    return {};
}

std::expected<executor::ExecutionResult, executor::ExecutionError> DatabaseEngine::execute(
    const physical_planner::plan::PhysicalPlan & plan
)
{
    if (transaction_manager_ == nullptr) {
        return std::unexpected(executor::ExecutionError {
            executor::ExecutionErrorCode::TransactionError,
            "Database transaction manager is not initialized",
        });
    }
    if (transaction_manager_->recovery_required()) {
        return std::unexpected(executor::ExecutionError {
            executor::ExecutionErrorCode::TransactionError,
            "Database requires WAL recovery before accepting more requests",
        });
    }

    executor::Executor executor {
        executor::ExecutionContext {
            .catalog = catalog(),
            .storage = storage_,
            .index_engine = index_engine_,
            .vector_index_engine = vector_index_engine_,
            .transaction_manager = *transaction_manager_,
        },
    };
    PlanExecutionDispatcher dispatcher {*this, executor};
    auto executed = dispatcher.dispatch(plan);
    if (executed && writes_wal(plan.kind())) {
        maybe_run_automatic_checkpoint();
    }
    return executed;
}

std::expected<executor::ExecutionResult, executor::ExecutionError> DatabaseEngine::commit_catalog_transaction(
    catalog::CatalogSnapshot snapshot,
    std::size_t affected_rows
)
{
    auto transaction = transaction_manager_->begin_implicit();
    if (!transaction) return std::unexpected(from_transaction_error(std::move(transaction.error())));
    auto staged = transaction_manager_->stage_catalog(*transaction, std::move(snapshot));
    if (!staged) {
        (void) transaction_manager_->abort(*transaction);
        return std::unexpected(from_transaction_error(std::move(staged.error())));
    }
    auto committed = transaction_manager_->commit(*transaction);
    if (!committed) return std::unexpected(from_transaction_error(std::move(committed.error())));
    return command_result(affected_rows);
}

std::expected<executor::ExecutionResult, executor::ExecutionError> DatabaseEngine::execute_create_database(
    const physical_planner::plan::CreateDatabasePlan & plan
)
{
    if (!plan.database_name().has_value()) {
        return command_result(0);
    }
    const auto existed = catalog().find_database(*plan.database_name()).has_value();

    auto editor = catalog::CatalogEditor::from(catalog());
    if (!editor) {
        return std::unexpected(from_catalog_error(std::move(editor.error())));
    }
    auto staged = std::move(*editor);

    auto created = staged.create_database(catalog::CreateDatabaseRequest {
        .database_name = *plan.database_name(),
    });
    if (!created.has_value()) {
        return std::unexpected(from_catalog_error(std::move(created.error())));
    }
    if (existed) return command_result(0);
    return commit_catalog_transaction(staged.snapshot(), 1);
}

std::expected<executor::ExecutionResult, executor::ExecutionError> DatabaseEngine::execute_create_collection(
    const physical_planner::plan::CreateCollectionPlan & plan
)
{
    if (!plan.collection_name().has_value()) {
        return command_result(0);
    }
    const auto existing = catalog().find_collection(plan.database_id(), *plan.collection_name());

    auto editor = catalog::CatalogEditor::from(catalog());
    if (!editor) {
        return std::unexpected(from_catalog_error(std::move(editor.error())));
    }
    auto staged = std::move(*editor);

    auto created = staged.create_collection(catalog::CreateCollectionRequest {
        .database_id = plan.database_id(),
        .collection_name = *plan.collection_name(),
        .columns = plan.columns(),
        .comment = plan.comment(),
    });
    if (!created.has_value()) {
        return std::unexpected(from_catalog_error(std::move(created.error())));
    }

    if (existing) {
        return command_result(0);
    }

    return commit_catalog_transaction(staged.snapshot(), 1);
}

std::expected<executor::ExecutionResult, executor::ExecutionError> DatabaseEngine::execute_create_index(
    const physical_planner::plan::CreateIndexPlan & plan
)
{
    if (!plan.index_name().has_value()) {
        return command_result(0);
    }
    const auto column = catalog().find_column(plan.column_id());
    if (!column) {
        return std::unexpected(executor::ExecutionError {
            executor::ExecutionErrorCode::InvalidPlan,
            "CREATE INDEX column was not found",
        });
    }
    const auto collection_id = column->collection_id();
    const auto existing = catalog().find_index(collection_id, *plan.index_name());

    auto editor = catalog::CatalogEditor::from(catalog());
    if (!editor) {
        return std::unexpected(from_catalog_error(std::move(editor.error())));
    }
    auto staged = std::move(*editor);

    auto created = staged.create_index(catalog::CreateIndexRequest {
        .collection_id = collection_id,
        .column_id = plan.column_id(),
        .index_name = *plan.index_name(),
        .kind = plan.index_kind(),
        .unique = plan.unique(),
    });
    if (!created.has_value()) {
        return std::unexpected(from_catalog_error(std::move(created.error())));
    }

    if (existing) {
        return command_result(0);
    }

    return commit_catalog_transaction(staged.snapshot(), 1);
}

std::expected<executor::ExecutionResult, executor::ExecutionError> DatabaseEngine::execute_create_vector_index(
    const physical_planner::plan::CreateVectorIndexPlan & plan
)
{
    if (!plan.index_name().has_value()) {
        return command_result(0);
    }
    const auto column = catalog().find_column(plan.column_id());
    if (!column) {
        return std::unexpected(executor::ExecutionError {
            executor::ExecutionErrorCode::InvalidPlan,
            "CREATE VECTOR INDEX column was not found",
        });
    }
    const auto collection_id = column->collection_id();
    const auto existing = catalog().find_vector_index(collection_id, *plan.index_name());

    auto editor = catalog::CatalogEditor::from(catalog());
    if (!editor) {
        return std::unexpected(from_catalog_error(std::move(editor.error())));
    }
    auto staged = std::move(*editor);

    auto created = staged.create_vector_index(catalog::CreateVectorIndexRequest {
        .collection_id = collection_id,
        .column_id = plan.column_id(),
        .vector_index_name = *plan.index_name(),
        .kind = plan.index_kind(),
        .metric = plan.metric(),
        .hnsw_options = {
            .max_neighbors = plan.max_neighbors(),
            .ef_construction = plan.ef_construction(),
            .ef_search_default = plan.ef_search_default(),
            .random_seed = plan.random_seed(),
        },
    });
    if (!created.has_value()) {
        return std::unexpected(from_catalog_error(std::move(created.error())));
    }

    if (existing) {
        return command_result(0);
    }

    return commit_catalog_transaction(staged.snapshot(), 1);
}

std::expected<executor::ExecutionResult, executor::ExecutionError> DatabaseEngine::execute_drop_database(
    const physical_planner::plan::DropDatabasePlan & plan
)
{
    if (!plan.database_id().has_value()) {
        return command_result(0);
    }
    const auto database = catalog().find_database(*plan.database_id());
    if (!database) {
        return std::unexpected(executor::ExecutionError {
            executor::ExecutionErrorCode::InvalidPlan,
            "DROP DATABASE target was not found",
        });
    }

    auto editor = catalog::CatalogEditor::from(catalog());
    if (!editor) {
        return std::unexpected(from_catalog_error(std::move(editor.error())));
    }
    auto staged = std::move(*editor);

    auto dropped = staged.drop_database(catalog::DropDatabaseRequest {
        .database_id = database->id(),
    });
    if (!dropped.has_value()) {
        return std::unexpected(from_catalog_error(std::move(dropped.error())));
    }

    return commit_catalog_transaction(staged.snapshot(), 1);
}

std::expected<executor::ExecutionResult, executor::ExecutionError> DatabaseEngine::execute_drop_collection(
    const physical_planner::plan::DropCollectionPlan & plan
)
{
    if (!plan.collection_id().has_value()) {
        return command_result(0);
    }
    const auto collection = catalog().find_collection(*plan.collection_id());
    if (!collection) {
        return std::unexpected(executor::ExecutionError {
            executor::ExecutionErrorCode::InvalidPlan,
            "DROP COLLECTION target was not found",
        });
    }

    auto editor = catalog::CatalogEditor::from(catalog());
    if (!editor) {
        return std::unexpected(from_catalog_error(std::move(editor.error())));
    }
    auto staged = std::move(*editor);

    auto dropped = staged.drop_collection(catalog::DropCollectionRequest {
        .collection_id = collection->id(),
    });
    if (!dropped.has_value()) {
        return std::unexpected(from_catalog_error(std::move(dropped.error())));
    }

    return commit_catalog_transaction(staged.snapshot(), 1);
}

std::expected<executor::ExecutionResult, executor::ExecutionError> DatabaseEngine::execute_drop_index(
    const physical_planner::plan::DropIndexPlan & plan
)
{
    if (!plan.index_id().has_value()) {
        return command_result(0);
    }
    const auto existing = catalog().find_index(*plan.index_id());
    if (!existing) {
        return std::unexpected(executor::ExecutionError {
            executor::ExecutionErrorCode::InvalidPlan,
            "DROP INDEX target was not found",
        });
    }
    auto editor = catalog::CatalogEditor::from(catalog());
    if (!editor) {
        return std::unexpected(from_catalog_error(std::move(editor.error())));
    }
    auto staged = std::move(*editor);

    auto dropped = staged.drop_index(catalog::DropIndexRequest {
        .index_id = existing->id(),
    });
    if (!dropped.has_value()) {
        return std::unexpected(from_catalog_error(std::move(dropped.error())));
    }

    return commit_catalog_transaction(staged.snapshot(), 1);
}

std::expected<executor::ExecutionResult, executor::ExecutionError> DatabaseEngine::execute_drop_vector_index(
    const physical_planner::plan::DropVectorIndexPlan & plan
)
{
    if (!plan.index_id().has_value()) {
        return command_result(0);
    }
    const auto existing = catalog().find_vector_index(*plan.index_id());
    if (!existing) {
        return std::unexpected(executor::ExecutionError {
            executor::ExecutionErrorCode::InvalidPlan,
            "DROP VECTOR INDEX target was not found",
        });
    }

    auto editor = catalog::CatalogEditor::from(catalog());
    if (!editor) {
        return std::unexpected(from_catalog_error(std::move(editor.error())));
    }
    auto staged = std::move(*editor);

    auto dropped = staged.drop_vector_index(catalog::DropVectorIndexRequest {
        .vector_index_id = existing->id(),
    });
    if (!dropped.has_value()) {
        return std::unexpected(from_catalog_error(std::move(dropped.error())));
    }

    return commit_catalog_transaction(staged.snapshot(), 1);
}

std::expected<void, storage::StorageError> DatabaseEngine::restore_storage_from_catalog()
{
    storage_.clear();
    for (const auto & database_reference : catalog().list_databases()) {
        const auto & database = database_reference.get();
        for (const auto & collection_reference : catalog().list_collections(database.id())) {
            const auto & collection = collection_reference.get();

            auto collection_schema = storage::load_collection_schema(catalog(), collection.id());
            if (!collection_schema.has_value()) {
                return std::unexpected(storage::make_storage_error(
                    storage::StorageErrorCode::InvalidFormat,
                    std::move(collection_schema.error().message()),
                    {
                        .operation = storage::StorageOperation::Load,
                        .collection_id = collection.id(),
                    }
                ));
            }

            auto opened = storage_.open_collection(std::move(*collection_schema));
            if (!opened.has_value()) {
                return std::unexpected(std::move(opened.error()));
            }
        }
    }
    return {};
}

executor::ExecutionError DatabaseEngine::from_catalog_error(
    catalog::CatalogError error
)
{
    auto message = error.message();
    return executor::ExecutionError {
        executor::ExecutionErrorCode::CatalogError,
        message,

        std::move(error),
    };
}

executor::ExecutionError DatabaseEngine::from_schema_error(
    storage::SchemaLoadError error
)
{
    auto message = error.message();
    return executor::ExecutionError {
        executor::ExecutionErrorCode::SchemaError,
        message,

        std::move(error),
    };
}

executor::ExecutionError DatabaseEngine::from_storage_error(
    storage::StorageError error
)
{
    auto message = error.message();
    return executor::ExecutionError {
        executor::ExecutionErrorCode::StorageError,
        message,

        std::move(error),
    };
}

executor::ExecutionError DatabaseEngine::from_index_error(
    index::IndexError error
)
{
    auto message = error.message();
    return executor::ExecutionError {
        executor::ExecutionErrorCode::IndexError,
        message,

        std::move(error),
    };
}

executor::ExecutionError DatabaseEngine::from_vector_index_error(
    vindex::VectorIndexError error
)
{
    auto message = error.message();
    return executor::ExecutionError {
        executor::ExecutionErrorCode::IndexError,
        message,

        std::move(error),
    };
}

} // namespace litedb::core::database

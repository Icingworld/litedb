#include "core/transaction/transaction_manager.hpp"

#include <algorithm>
#include <chrono>
#include <limits>
#include <set>
#include <system_error>
#include <utility>

#include "core/storage/schema_loader.hpp"
#include "core/meta/meta_store.hpp"
#include "core/transaction/transaction_file_overlay.hpp"

namespace litedb::core::transaction
{

namespace
{

/**
 * @brief 事务暂存清理器
 */
class StagingCleanup final
{
public:
    explicit StagingCleanup(std::filesystem::path path)
        : path_(std::move(path))
    {
    }

    ~StagingCleanup()
    {
        std::error_code ignored;
        std::filesystem::remove_all(path_, ignored);
    }

private:
    std::filesystem::path path_;    ///< 暂存路径
};

class CommitTimer final
{
public:
    explicit CommitTimer(std::function<void(std::uint64_t)> callback)
        : callback_(std::move(callback))
        , started_(std::chrono::steady_clock::now())
    {
    }

    ~CommitTimer()
    {
        const auto elapsed = std::chrono::duration_cast<std::chrono::microseconds>(
            std::chrono::steady_clock::now() - started_
        );
        callback_(static_cast<std::uint64_t>(elapsed.count()));
    }

private:
    std::function<void(std::uint64_t)> callback_;
    std::chrono::steady_clock::time_point started_;
};

// TODO: 文件操作从 std::fstream 迁移到使用 filesystem 模块提供的 API。

/**
 * @brief 复制目录
 * @param source 源目录
 * @param destination 目标目录
 * @return 是否成功
 * @details 复制目录到目标目录
 */
/**
 * @brief 读取文件
 * @param path 文件路径
 * @return 文件内容
 * @details 读取文件内容
 */
/**
 * @brief 生成子系统错误信息
 * @param subsystem 子系统
 * @param message 错误信息
 * @return 错误信息
 * @details 生成子系统错误信息
 */
std::string mutation_error(std::string subsystem, std::string message)
{
    return std::move(subsystem) + " transaction preparation failed: " + std::move(message);
}

} // namespace

TransactionManager::TransactionManager(
    std::filesystem::path data_directory,
    filesystem::FileSystem & filesystem,
    meta::CatalogPublisher & catalog,
    storage::StorageEngine & storage,
    index::IndexEngine & index_engine,
    vindex::VectorIndexEngine & vector_index_engine,
    wal::WalManager & wal,
    TransactionId maximum_recovered_transaction_id,
    TransactionOptions options
) noexcept
    : data_directory_(std::move(data_directory))
    , filesystem_(&filesystem)
    , catalog_(&catalog)
    , storage_(&storage)
    , index_engine_(&index_engine)
    , vector_index_engine_(&vector_index_engine)
    , wal_(&wal)
    , next_transaction_id_(maximum_recovered_transaction_id == std::numeric_limits<TransactionId>::max()
                               ? InvalidTransactionId
                               : maximum_recovered_transaction_id + 1)
    , options_(std::move(options))
{
    wal_size_bytes_.store(wal_->metrics().size_bytes, std::memory_order_relaxed);
}

std::expected<void, std::string> sync_file(
    filesystem::FileSystem & filesystem,
    const std::filesystem::path & path
)
{
    auto opened = filesystem.open(
        path,
        {
            filesystem::FileAccess::ReadWrite,
            filesystem::FileCreateMode::OpenExisting,
        }
    );
    if (!opened) return std::unexpected(opened.error().message());
    auto synced = opened->sync_all();
    if (!synced) return std::unexpected(synced.error().message());
    return {};
}

std::expected<void, std::string> sync_directory_if_supported(
    filesystem::FileSystem & filesystem,
    const std::filesystem::path & path
)
{
    auto synced = filesystem.sync_directory(path);
    if (!synced && !synced.error().is(filesystem::FileSystemErrorCode::Unsupported)) {
        return std::unexpected(synced.error().message());
    }
    return {};
}

std::vector<wal::FileTarget> catalog_physical_targets(meta::CatalogView catalog)
{
    std::vector<wal::FileTarget> targets;
    for (const auto * database : catalog.list_databases()) {
        if (database == nullptr) continue;
        for (const auto * collection : catalog.list_collections(database->id())) {
            if (collection == nullptr) continue;
            targets.push_back({.kind = wal::FileKind::CollectionStore, .object_id = collection->id()});
            for (const auto * index : catalog.list_indexes(collection->id())) {
                if (index != nullptr) {
                    targets.push_back({.kind = wal::FileKind::ScalarIndex, .object_id = index->id()});
                }
            }
            for (const auto * index : catalog.list_vector_indexes(collection->id())) {
                if (index != nullptr && index->index_kind() == meta::entry::VectorIndexKind::Hnsw) {
                    targets.push_back({.kind = wal::FileKind::VectorIndex, .object_id = index->id()});
                }
            }
        }
    }
    return targets;
}

bool contains_target(const std::vector<wal::FileTarget> & targets, const wal::FileTarget & target)
{
    return std::find(targets.begin(), targets.end(), target) != targets.end();
}

bool TransactionManager::recovery_required() const noexcept
{
    return recovery_required_.load(std::memory_order_acquire);
}

TransactionMetrics TransactionManager::metrics() const noexcept
{
    const auto wal_metrics = wal_->metrics();
    return TransactionMetrics {
        .started_transactions = started_transactions_.load(std::memory_order_relaxed),
        .committed_transactions = committed_transactions_.load(std::memory_order_relaxed),
        .aborted_transactions = aborted_transactions_.load(std::memory_order_relaxed),
        .failed_commits = failed_commits_.load(std::memory_order_relaxed),
        .total_commit_duration_us = total_commit_duration_us_.load(std::memory_order_relaxed),
        .last_commit_duration_us = last_commit_duration_us_.load(std::memory_order_relaxed),
        .maximum_commit_duration_us = maximum_commit_duration_us_.load(std::memory_order_relaxed),
        .wal_size_bytes = wal_size_bytes_.load(std::memory_order_relaxed),
        .wal_generation = wal_metrics.generation,
        .checkpoint_transaction_id = wal_metrics.checkpoint_transaction_id,
        .completed_checkpoints = completed_checkpoints_.load(std::memory_order_relaxed),
        .failed_checkpoints = failed_checkpoints_.load(std::memory_order_relaxed),
        .last_checkpoint_duration_us = last_checkpoint_duration_us_.load(std::memory_order_relaxed),
        .reclaimed_wal_bytes = reclaimed_wal_bytes_.load(std::memory_order_relaxed),
    };
}

void TransactionManager::record_commit_duration(std::uint64_t duration_us) noexcept
{
    last_commit_duration_us_.store(duration_us, std::memory_order_relaxed);
    total_commit_duration_us_.fetch_add(duration_us, std::memory_order_relaxed);
    auto maximum = maximum_commit_duration_us_.load(std::memory_order_relaxed);
    while (duration_us > maximum &&
           !maximum_commit_duration_us_.compare_exchange_weak(maximum, duration_us, std::memory_order_relaxed)) {
    }
    wal_size_bytes_.store(wal_->metrics().size_bytes, std::memory_order_relaxed);
}

std::expected<void, TransactionError> TransactionManager::sync_checkpoint_participants(
    TransactionId checkpoint_transaction_id
)
{
    auto synced_meta = sync_file(*filesystem_, data_directory_ / "meta.lmeta");
    if (!synced_meta) {
        return std::unexpected(error(TransactionErrorCode::ApplyFailed, checkpoint_transaction_id,
                                     "Failed to sync meta participant: " + std::move(synced_meta.error())));
    }

    for (const auto & target : catalog_physical_targets(catalog_->view())) {
        const auto path = wal::FileWriteBatch::resolve_target(data_directory_, target);
        auto synced = sync_file(*filesystem_, path);
        if (!synced) {
            return std::unexpected(error(TransactionErrorCode::ApplyFailed, checkpoint_transaction_id,
                                         "Failed to sync checkpoint participant " + path.string() + ": " +
                                             std::move(synced.error())));
        }
    }

    for (const auto * name : {"collections", "indexes", "vindexes"}) {
        const auto path = data_directory_ / name;
        auto created = filesystem_->create_dir_all(path);
        if (!created) {
            return std::unexpected(error(TransactionErrorCode::ApplyFailed, checkpoint_transaction_id,
                                         "Failed to create checkpoint directory: " + created.error().message()));
        }
        auto synced = sync_directory_if_supported(*filesystem_, path);
        if (!synced) {
            return std::unexpected(error(TransactionErrorCode::ApplyFailed, checkpoint_transaction_id,
                                         "Failed to sync checkpoint directory: " + std::move(synced.error())));
        }
    }
    auto root_synced = sync_directory_if_supported(*filesystem_, data_directory_);
    if (!root_synced) {
        return std::unexpected(error(TransactionErrorCode::ApplyFailed, checkpoint_transaction_id,
                                     "Failed to sync data directory: " + std::move(root_synced.error())));
    }
    return {};
}

std::expected<void, TransactionError> TransactionManager::checkpoint()
{
    const auto started = std::chrono::steady_clock::now();
    std::unique_lock writer_guard {writer_mutex_};
    const auto checkpoint_transaction_id = next_transaction_id_ == 1
                                               ? InvalidTransactionId
                                               : next_transaction_id_ - 1;
    auto finish = [this, started](bool success) {
        const auto elapsed = std::chrono::duration_cast<std::chrono::microseconds>(
            std::chrono::steady_clock::now() - started
        );
        last_checkpoint_duration_us_.store(static_cast<std::uint64_t>(elapsed.count()), std::memory_order_relaxed);
        if (success) completed_checkpoints_.fetch_add(1, std::memory_order_relaxed);
        else failed_checkpoints_.fetch_add(1, std::memory_order_relaxed);
    };

    if (recovery_required()) {
        finish(false);
        return std::unexpected(error(TransactionErrorCode::RecoveryRequired, checkpoint_transaction_id,
                                     "Database requires recovery before checkpoint"));
    }

    const auto before_bytes = wal_->metrics().size_bytes;
    auto wal_flushed = wal_->flush_all();
    if (!wal_flushed) {
        finish(false);
        return std::unexpected(error(TransactionErrorCode::WalError, checkpoint_transaction_id,
                                     "Failed to flush WAL before checkpoint: " + wal_flushed.error().message));
    }
    if (options_.checkpoint_stage_hook) {
        options_.checkpoint_stage_hook(CheckpointStage::AfterWalFlush, checkpoint_transaction_id);
    }
    auto participants_synced = sync_checkpoint_participants(checkpoint_transaction_id);
    if (!participants_synced) {
        finish(false);
        return participants_synced;
    }
    if (options_.checkpoint_stage_hook) {
        options_.checkpoint_stage_hook(CheckpointStage::AfterParticipantSync, checkpoint_transaction_id);
    }
    auto rotated = wal_->rotate(
        checkpoint_transaction_id,
        [this, checkpoint_transaction_id](wal::WalRotationStage stage) {
            if (!options_.checkpoint_stage_hook) return;
            CheckpointStage checkpoint_stage = CheckpointStage::AfterTemporaryWalSync;
            switch (stage) {
            case wal::WalRotationStage::AfterTemporarySync:
                checkpoint_stage = CheckpointStage::AfterTemporaryWalSync;
                break;
            case wal::WalRotationStage::AfterPublish:
                checkpoint_stage = CheckpointStage::AfterWalPublish;
                break;
            case wal::WalRotationStage::AfterDirectorySync:
                checkpoint_stage = CheckpointStage::AfterWalDirectorySync;
                break;
            case wal::WalRotationStage::AfterSwitch:
                checkpoint_stage = CheckpointStage::AfterWalSwitch;
                break;
            case wal::WalRotationStage::AfterOldSegmentRemoval:
                checkpoint_stage = CheckpointStage::AfterOldWalRemoval;
                break;
            }
            options_.checkpoint_stage_hook(checkpoint_stage, checkpoint_transaction_id);
        }
    );
    if (!rotated) {
        recovery_required_.store(true, std::memory_order_release);
        finish(false);
        return std::unexpected(error(TransactionErrorCode::RecoveryRequired, checkpoint_transaction_id,
                                     "WAL rotation outcome is indeterminate: " + rotated.error().message));
    }

    const auto after_bytes = wal_->metrics().size_bytes;
    reclaimed_wal_bytes_.fetch_add(before_bytes > after_bytes ? before_bytes - after_bytes : 0,
                                   std::memory_order_relaxed);
    wal_size_bytes_.store(after_bytes, std::memory_order_relaxed);
    finish(true);
    return {};
}

TransactionError TransactionManager::error(
    TransactionErrorCode code,
    TransactionId id,
    std::string message
) const
{
    return TransactionError {.code = code, .transaction_id = id, .message = std::move(message)};
}

std::expected<TransactionContext, TransactionError> TransactionManager::begin_implicit()
{
    std::unique_lock writer_guard {writer_mutex_};
    if (recovery_required()) {
        return std::unexpected(error(TransactionErrorCode::RecoveryRequired, InvalidTransactionId,
                                     "Database requires WAL recovery before accepting transactions"));
    }
    if (next_transaction_id_ == InvalidTransactionId || next_transaction_id_ == std::numeric_limits<TransactionId>::max()) {
        return std::unexpected(error(TransactionErrorCode::InvalidState, InvalidTransactionId,
                                     "Transaction ID space is exhausted"));
    }
    TransactionContext transaction {next_transaction_id_++};
    transaction.writer_guard_ = std::move(writer_guard);
    transaction.owner_ = this;
    started_transactions_.fetch_add(1, std::memory_order_relaxed);
    return transaction;
}

bool TransactionManager::failpoint(CommitStage stage, TransactionContext & transaction, bool durable)
{
    if (!options_.commit_stage_hook || !options_.commit_stage_hook(stage, transaction.id())) {
        return false;
    }
    if (durable) {
        recovery_required_.store(true, std::memory_order_release);
        transaction.release_writer_guard();
    } else {
        transaction.state_ = TransactionState::Aborting;
        (void) abort(transaction);
    }
    return true;
}

std::expected<void, TransactionError> TransactionManager::stage_insert(
    TransactionContext & transaction,
    common::CollectionId collection_id,
    common::RecordData after
)
{
    if (transaction.owner_ != this || !transaction.writer_guard_.owns_lock() ||
        transaction.state() != TransactionState::Active || transaction.rollback_only()) {
        return std::unexpected(error(TransactionErrorCode::InvalidState, transaction.id(), "Cannot stage insert"));
    }
    transaction.stage(RowMutation {
        .kind = RowMutationKind::Insert,
        .collection_id = collection_id,
        .record_id = 0,
        .before = std::nullopt,
        .after = std::move(after),
    });
    return {};
}

std::expected<void, TransactionError> TransactionManager::stage_update(
    TransactionContext & transaction,
    common::CollectionId collection_id,
    common::RecordId record_id,
    common::RecordData before,
    common::RecordData after
)
{
    if (transaction.owner_ != this || !transaction.writer_guard_.owns_lock() ||
        transaction.state() != TransactionState::Active || transaction.rollback_only()) {
        return std::unexpected(error(TransactionErrorCode::InvalidState, transaction.id(), "Cannot stage update"));
    }
    transaction.stage(RowMutation {
        .kind = RowMutationKind::Update,
        .collection_id = collection_id,
        .record_id = record_id,
        .before = std::move(before),
        .after = std::move(after),
    });
    return {};
}

std::expected<void, TransactionError> TransactionManager::stage_delete(
    TransactionContext & transaction,
    common::CollectionId collection_id,
    common::RecordId record_id,
    common::RecordData before
)
{
    if (transaction.owner_ != this || !transaction.writer_guard_.owns_lock() ||
        transaction.state() != TransactionState::Active || transaction.rollback_only()) {
        return std::unexpected(error(TransactionErrorCode::InvalidState, transaction.id(), "Cannot stage delete"));
    }
    transaction.stage(RowMutation {
        .kind = RowMutationKind::Delete,
        .collection_id = collection_id,
        .record_id = record_id,
        .before = std::move(before),
        .after = std::nullopt,
    });
    return {};
}

std::expected<void, TransactionError> TransactionManager::stage_catalog(
    TransactionContext & transaction,
    meta::MetaSnapshot snapshot
)
{
    if (transaction.owner_ != this || !transaction.writer_guard_.owns_lock() ||
        transaction.state() != TransactionState::Active || transaction.rollback_only() ||
        !transaction.write_set().empty() || transaction.catalog_snapshot_) {
        return std::unexpected(error(TransactionErrorCode::InvalidState, transaction.id(), "Cannot stage catalog"));
    }
    auto validated = meta::build_catalog_state(snapshot);
    if (!validated) {
        return std::unexpected(error(
            TransactionErrorCode::PrepareFailed,
            transaction.id(),
            mutation_error("meta", validated.error().message())
        ));
    }
    transaction.catalog_snapshot_ = std::move(snapshot);
    return {};
}

std::expected<wal::FileWriteBatch, TransactionError> TransactionManager::prepare_catalog(
    const TransactionContext & transaction,
    const std::filesystem::path & staging_directory
)
{
    if (!transaction.catalog_snapshot()) {
        return std::unexpected(error(TransactionErrorCode::PrepareFailed, transaction.id(), "Catalog snapshot is absent"));
    }

    TransactionFileOverlay overlay {staging_directory, data_directory_, *filesystem_};
    auto & staging_filesystem = overlay.filesystem();

    meta::CatalogEditor after_catalog;
    auto after_catalog_editor = meta::CatalogEditor::from(*transaction.catalog_snapshot());
    if (!after_catalog_editor) {
        return std::unexpected(error(TransactionErrorCode::PrepareFailed, transaction.id(),
                                     mutation_error("meta", after_catalog_editor.error().message())));
    }
    after_catalog = std::move(*after_catalog_editor);
    const auto after_catalog_view = after_catalog.view();
    meta::MetaStore staged_meta {staging_directory / "meta.lmeta", staging_filesystem};
    auto saved_meta = staged_meta.save(*transaction.catalog_snapshot());
    if (!saved_meta) {
        return std::unexpected(error(TransactionErrorCode::PrepareFailed, transaction.id(),
                                     mutation_error("meta", saved_meta.error().message())));
    }

    storage::StorageEngine staged_storage {
        staging_directory,
        staging_filesystem,
        storage::StorageOpenMode::TransactionalStaging,
    };
    for (const auto * database : after_catalog_view.list_databases()) {
        if (database == nullptr) continue;
        for (const auto * collection : after_catalog_view.list_collections(database->id())) {
            if (collection == nullptr) continue;
            auto schema = storage::load_collection_schema(after_catalog_view, collection->id());
            if (!schema) {
                return std::unexpected(error(TransactionErrorCode::PrepareFailed, transaction.id(),
                                             mutation_error("schema", std::move(schema.error().message))));
            }
            auto opened = catalog_->view().find_collection(collection->id()) != nullptr
                              ? staged_storage.open_collection(std::move(*schema))
                              : staged_storage.create_collection(std::move(*schema));
            if (!opened) {
                return std::unexpected(error(TransactionErrorCode::PrepareFailed, transaction.id(),
                                             mutation_error("storage", opened.error().message())));
            }
        }
    }

    {
        index::IndexEngine creator {staging_directory, staging_filesystem};
        for (const auto * database : after_catalog_view.list_databases()) {
            if (database == nullptr) continue;
            for (const auto * collection : after_catalog_view.list_collections(database->id())) {
                if (collection == nullptr) continue;
                auto schema = storage::load_collection_schema(after_catalog_view, collection->id());
                if (!schema) return std::unexpected(error(TransactionErrorCode::PrepareFailed, transaction.id(), std::move(schema.error().message)));
                for (const auto * index : after_catalog_view.list_indexes(collection->id())) {
                    if (index == nullptr || catalog_->view().find_index(index->id()) != nullptr) continue;
                    auto created = creator.create_index(*index, *schema, staged_storage);
                    if (!created) return std::unexpected(error(TransactionErrorCode::PrepareFailed, transaction.id(), mutation_error("scalar index", std::move(created.error().message))));
                }
            }
        }
    }
    index::IndexEngine staged_indexes {staging_directory, staging_filesystem};
    auto indexes_restored = staged_indexes.restore_all(after_catalog_view, staged_storage);
    if (!indexes_restored) {
        return std::unexpected(error(TransactionErrorCode::PrepareFailed, transaction.id(),
                                     mutation_error("scalar index", std::move(indexes_restored.error().message))));
    }

    {
        vindex::VectorIndexEngine creator {staging_directory / "vindexes", staging_filesystem};
        for (const auto * database : after_catalog_view.list_databases()) {
            if (database == nullptr) continue;
            for (const auto * collection : after_catalog_view.list_collections(database->id())) {
                if (collection == nullptr) continue;
                auto schema = storage::load_collection_schema(after_catalog_view, collection->id());
                if (!schema) return std::unexpected(error(TransactionErrorCode::PrepareFailed, transaction.id(), std::move(schema.error().message)));
                for (const auto * index : after_catalog_view.list_vector_indexes(collection->id())) {
                    if (index == nullptr || catalog_->view().find_vector_index(index->id()) != nullptr) continue;
                    auto created = creator.create_index(*index, *schema, staged_storage);
                    if (!created) return std::unexpected(error(TransactionErrorCode::PrepareFailed, transaction.id(), mutation_error("vector index", std::move(created.error().message))));
                }
            }
        }
    }
    vindex::VectorIndexEngine staged_vectors {staging_directory / "vindexes", staging_filesystem};
    auto vectors_restored = staged_vectors.restore_all(after_catalog_view, staged_storage);
    if (!vectors_restored) {
        return std::unexpected(error(TransactionErrorCode::PrepareFailed, transaction.id(),
                                     mutation_error("vector index", std::move(vectors_restored.error().message))));
    }

    const auto before_targets = catalog_physical_targets(catalog_->view());
    const auto after_targets = catalog_physical_targets(after_catalog_view);
    for (const auto & target : before_targets) {
        if (!contains_target(after_targets, target)) {
            auto removed = staging_filesystem.remove(
                wal::FileWriteBatch::resolve_target(staging_directory, target)
            );
            if (!removed) {
                return std::unexpected(error(
                    TransactionErrorCode::PrepareFailed,
                    transaction.id(),
                    mutation_error("overlay", removed.error().message())
                ));
            }
        }
    }
    auto batch = overlay.export_batch();
    if (!batch) {
        return std::unexpected(error(
            TransactionErrorCode::PrepareFailed,
            transaction.id(),
            mutation_error("overlay", batch.error().message())
        ));
    }
    return std::move(*batch);
}

std::expected<wal::FileWriteBatch, TransactionError> TransactionManager::prepare(
    const TransactionContext & transaction,
    const std::filesystem::path & staging_directory
)
{
    if (transaction.catalog_snapshot()) {
        return prepare_catalog(transaction, staging_directory);
    }
    TransactionFileOverlay overlay {staging_directory, data_directory_, *filesystem_};
    auto & staging_filesystem = overlay.filesystem();
    std::set<common::CollectionId> collections;
    for (const auto & mutation : transaction.write_set()) collections.insert(mutation.collection_id);

    {
        storage::StorageEngine staged_storage {
            staging_directory,
            staging_filesystem,
            storage::StorageOpenMode::TransactionalStaging,
        };
        for (const auto collection_id : collections) {
            auto collection_schema = storage::load_collection_schema(catalog_->view(), collection_id);
            if (!collection_schema) {
                return std::unexpected(error(TransactionErrorCode::PrepareFailed, transaction.id(),
                                             mutation_error("schema", std::move(collection_schema.error().message))));
            }
            auto opened = staged_storage.open_collection(std::move(*collection_schema));
            if (!opened) {
                return std::unexpected(error(TransactionErrorCode::PrepareFailed, transaction.id(),
                                             mutation_error("storage", opened.error().message())));
            }
        }
        index::IndexEngine staged_indexes {staging_directory, staging_filesystem};
        vindex::VectorIndexEngine staged_vectors {staging_directory / "vindexes", staging_filesystem};
        for (const auto collection_id : collections) {
            auto indexes_restored = staged_indexes.reload_collection(catalog_->view(), staged_storage, collection_id);
            if (!indexes_restored) {
                return std::unexpected(error(TransactionErrorCode::PrepareFailed, transaction.id(),
                                             mutation_error("scalar index", std::move(indexes_restored.error().message))));
            }
            auto vectors_restored = staged_vectors.reload_collection(catalog_->view(), staged_storage, collection_id);
            if (!vectors_restored) {
                return std::unexpected(error(TransactionErrorCode::PrepareFailed, transaction.id(),
                                             mutation_error("vector index", std::move(vectors_restored.error().message))));
            }
        }

        for (const auto & mutation : transaction.write_set()) {
            switch (mutation.kind) {
            case RowMutationKind::Insert: {
                if (!mutation.after) {
                    return std::unexpected(error(TransactionErrorCode::PrepareFailed, transaction.id(), "Insert has no after image"));
                }
                auto scalar = staged_indexes.prepare_insert(mutation.collection_id, *mutation.after);
                if (!scalar) return std::unexpected(error(TransactionErrorCode::PrepareFailed, transaction.id(), mutation_error("scalar index", std::move(scalar.error().message))));
                auto vector = staged_vectors.prepare_insert(mutation.collection_id, *mutation.after);
                if (!vector) return std::unexpected(error(TransactionErrorCode::PrepareFailed, transaction.id(), mutation_error("vector index", std::move(vector.error().message))));
                auto inserted = staged_storage.insert(mutation.collection_id, *mutation.after);
                if (!inserted) return std::unexpected(error(TransactionErrorCode::PrepareFailed, transaction.id(), mutation_error("storage", inserted.error().message())));
                auto scalar_applied = staged_indexes.on_insert(*inserted, *scalar);
                if (!scalar_applied) return std::unexpected(error(TransactionErrorCode::PrepareFailed, transaction.id(), mutation_error("scalar index", std::move(scalar_applied.error().message))));
                auto vector_applied = staged_vectors.on_insert(*inserted, *vector);
                if (!vector_applied) return std::unexpected(error(TransactionErrorCode::PrepareFailed, transaction.id(), mutation_error("vector index", std::move(vector_applied.error().message))));
                break;
            }
            case RowMutationKind::Update: {
                if (!mutation.before || !mutation.after) {
                    return std::unexpected(error(TransactionErrorCode::PrepareFailed, transaction.id(), "Update images are incomplete"));
                }
                auto scalar = staged_indexes.prepare_update(mutation.collection_id, *mutation.before, *mutation.after);
                if (!scalar) return std::unexpected(error(TransactionErrorCode::PrepareFailed, transaction.id(), mutation_error("scalar index", std::move(scalar.error().message))));
                auto vector = staged_vectors.prepare_update(mutation.collection_id, *mutation.before, *mutation.after);
                if (!vector) return std::unexpected(error(TransactionErrorCode::PrepareFailed, transaction.id(), mutation_error("vector index", std::move(vector.error().message))));
                auto stored = staged_storage.update(mutation.collection_id, mutation.record_id, *mutation.after);
                if (!stored) return std::unexpected(error(TransactionErrorCode::PrepareFailed, transaction.id(), mutation_error("storage", stored.error().message())));
                auto scalar_applied = staged_indexes.on_update(mutation.record_id, *scalar);
                if (!scalar_applied) return std::unexpected(error(TransactionErrorCode::PrepareFailed, transaction.id(), mutation_error("scalar index", std::move(scalar_applied.error().message))));
                auto vector_applied = staged_vectors.on_update(mutation.record_id, *vector);
                if (!vector_applied) return std::unexpected(error(TransactionErrorCode::PrepareFailed, transaction.id(), mutation_error("vector index", std::move(vector_applied.error().message))));
                break;
            }
            case RowMutationKind::Delete: {
                if (!mutation.before) {
                    return std::unexpected(error(TransactionErrorCode::PrepareFailed, transaction.id(), "Delete has no before image"));
                }
                auto scalar = staged_indexes.prepare_delete(mutation.collection_id, *mutation.before);
                if (!scalar) return std::unexpected(error(TransactionErrorCode::PrepareFailed, transaction.id(), mutation_error("scalar index", std::move(scalar.error().message))));
                auto vector = staged_vectors.prepare_delete(mutation.collection_id, *mutation.before);
                if (!vector) return std::unexpected(error(TransactionErrorCode::PrepareFailed, transaction.id(), mutation_error("vector index", std::move(vector.error().message))));
                auto vector_applied = staged_vectors.on_delete(mutation.record_id, *vector);
                if (!vector_applied) return std::unexpected(error(TransactionErrorCode::PrepareFailed, transaction.id(), mutation_error("vector index", std::move(vector_applied.error().message))));
                auto scalar_applied = staged_indexes.on_delete(mutation.record_id, *scalar);
                if (!scalar_applied) return std::unexpected(error(TransactionErrorCode::PrepareFailed, transaction.id(), mutation_error("scalar index", std::move(scalar_applied.error().message))));
                auto stored = staged_storage.erase(mutation.collection_id, mutation.record_id);
                if (!stored) return std::unexpected(error(TransactionErrorCode::PrepareFailed, transaction.id(), mutation_error("storage", stored.error().message())));
                break;
            }
            }
        }
    }

    auto batch = overlay.export_batch();
    if (!batch) {
        return std::unexpected(error(
            TransactionErrorCode::PrepareFailed,
            transaction.id(),
            mutation_error("overlay", batch.error().message())
        ));
    }
    return std::move(*batch);
}

std::expected<void, TransactionError> TransactionManager::reload_runtime(const TransactionContext & transaction)
{
    const auto transaction_id = transaction.id();
    if (!transaction.catalog_snapshot()) {
        std::set<common::CollectionId> collections;
        for (const auto & mutation : transaction.write_set()) collections.insert(mutation.collection_id);
        for (const auto collection_id : collections) {
            auto collection_schema = storage::load_collection_schema(catalog_->view(), collection_id);
            if (!collection_schema) {
                return std::unexpected(error(TransactionErrorCode::ApplyFailed, transaction_id,
                                             std::move(collection_schema.error().message)));
            }
            auto storage_reloaded = storage_->reload_collection(std::move(*collection_schema));
            if (!storage_reloaded) {
                return std::unexpected(error(TransactionErrorCode::ApplyFailed, transaction_id,
                                             storage_reloaded.error().message()));
            }
            auto indexes_reloaded = index_engine_->reload_collection(catalog_->view(), *storage_, collection_id);
            if (!indexes_reloaded) {
                return std::unexpected(error(TransactionErrorCode::ApplyFailed, transaction_id,
                                             std::move(indexes_reloaded.error().message)));
            }
            auto vectors_reloaded = vector_index_engine_->reload_collection(catalog_->view(), *storage_, collection_id);
            if (!vectors_reloaded) {
                return std::unexpected(error(TransactionErrorCode::ApplyFailed, transaction_id,
                                             std::move(vectors_reloaded.error().message)));
            }
        }
        return {};
    }

    storage::StorageEngine restored_storage {data_directory_, *filesystem_};
    const auto catalog_view = catalog_->view();
    for (const auto * database : catalog_view.list_databases()) {
        if (database == nullptr) continue;
        for (const auto * collection : catalog_view.list_collections(database->id())) {
            if (collection == nullptr) continue;
            auto schema = storage::load_collection_schema(catalog_->view(), collection->id());
            if (!schema) return std::unexpected(error(TransactionErrorCode::ApplyFailed, transaction_id, std::move(schema.error().message)));
            auto opened = restored_storage.open_collection(std::move(*schema));
            if (!opened) return std::unexpected(error(TransactionErrorCode::ApplyFailed, transaction_id, opened.error().message()));
        }
    }
    index::IndexEngine restored_indexes {data_directory_, *filesystem_};
    auto indexes = restored_indexes.restore_all(catalog_->view(), restored_storage);
    if (!indexes) return std::unexpected(error(TransactionErrorCode::ApplyFailed, transaction_id, std::move(indexes.error().message)));
    *storage_ = std::move(restored_storage);
    *index_engine_ = std::move(restored_indexes);

    // FlatIndex keeps a non-owning StorageEngine pointer, so construct the
    // replacement vector engine only after the live storage object is published.
    vindex::VectorIndexEngine restored_vectors {data_directory_ / "vindexes", *filesystem_};
    auto vectors = restored_vectors.restore_all(catalog_->view(), *storage_);
    if (!vectors) return std::unexpected(error(TransactionErrorCode::ApplyFailed, transaction_id, std::move(vectors.error().message)));
    *vector_index_engine_ = std::move(restored_vectors);
    return {};
}

std::expected<void, TransactionError> TransactionManager::commit(TransactionContext & transaction)
{
    bool commit_succeeded = false;
    CommitTimer timer {[this, &commit_succeeded](std::uint64_t duration_us) {
        record_commit_duration(duration_us);
        if (!commit_succeeded) {
            failed_commits_.fetch_add(1, std::memory_order_relaxed);
        }
    }};

    if (transaction.owner_ != this || !transaction.writer_guard_.owns_lock()) {
        return std::unexpected(error(TransactionErrorCode::InvalidState, transaction.id(),
                                     "Transaction was not started by this TransactionManager"));
    }
    if (recovery_required()) {
        return std::unexpected(error(TransactionErrorCode::RecoveryRequired, transaction.id(), "Database requires recovery"));
    }
    if (transaction.rollback_only()) {
        auto aborted = abort(transaction);
        return std::unexpected(error(TransactionErrorCode::RollbackOnly, transaction.id(),
                                     transaction.failure() ? transaction.failure()->message : "Transaction is rollback-only"));
    }
    if (!transaction.transition_to(TransactionState::Preparing)) {
        return std::unexpected(error(TransactionErrorCode::InvalidState, transaction.id(), "Transaction cannot enter prepare"));
    }

    const auto staging_directory = data_directory_ / ".transactions" / ("txn_" + std::to_string(transaction.id()));
    StagingCleanup cleanup {staging_directory};
    auto batch = prepare(transaction, staging_directory);
    if (!batch) {
        transaction.mark_rollback_only(batch.error().message);
        (void) abort(transaction);
        return std::unexpected(std::move(batch.error()));
    }
    if (failpoint(CommitStage::AfterPrepare, transaction, false)) {
        return std::unexpected(error(TransactionErrorCode::FaultInjected, transaction.id(), "Injected failure after prepare"));
    }
    if (!transaction.transition_to(TransactionState::Committing)) {
        return std::unexpected(error(TransactionErrorCode::InvalidState, transaction.id(), "Transaction cannot enter commit"));
    }

    auto begin = wal_->append_begin(transaction.id());
    if (!begin) {
        transaction.state_ = TransactionState::Aborting;
        (void) abort(transaction);
        return std::unexpected(error(TransactionErrorCode::WalError, transaction.id(), std::move(begin.error().message)));
    }
    transaction.note_lsn(*begin);
    if (failpoint(CommitStage::AfterWalBegin, transaction, false)) {
        return std::unexpected(error(TransactionErrorCode::FaultInjected, transaction.id(), "Injected failure after WAL begin"));
    }
    for (const auto & write : batch->writes()) {
        auto appended = wal_->append_write(transaction.id(), write);
        if (!appended) {
            transaction.state_ = TransactionState::Aborting;
            (void) abort(transaction);
            return std::unexpected(error(TransactionErrorCode::WalError, transaction.id(), std::move(appended.error().message)));
        }
        transaction.note_lsn(*appended);
    }
    if (failpoint(CommitStage::AfterWalWrites, transaction, false)) {
        return std::unexpected(error(TransactionErrorCode::FaultInjected, transaction.id(), "Injected failure after WAL writes"));
    }
    auto committed = wal_->append_commit(transaction.id());
    if (!committed) {
        recovery_required_.store(true, std::memory_order_release);
        transaction.release_writer_guard();
        return std::unexpected(error(TransactionErrorCode::RecoveryRequired, transaction.id(),
                                     "WAL commit append outcome is indeterminate: " + committed.error().message));
    }
    transaction.note_commit_lsn(*committed);
    if (failpoint(CommitStage::AfterWalCommitAppend, transaction, true)) {
        return std::unexpected(error(TransactionErrorCode::RecoveryRequired, transaction.id(), "Injected failure after WAL commit append"));
    }
    auto flushed = wal_->flush_through(*committed);
    if (!flushed) {
        recovery_required_.store(true, std::memory_order_release);
        transaction.release_writer_guard();
        return std::unexpected(error(TransactionErrorCode::RecoveryRequired, transaction.id(),
                                     "WAL commit durability is indeterminate: " + flushed.error().message));
    }
    if (!transaction.transition_to(TransactionState::Committed)) {
        recovery_required_.store(true, std::memory_order_release);
        transaction.release_writer_guard();
        return std::unexpected(error(TransactionErrorCode::RecoveryRequired, transaction.id(), "Durable transaction has invalid state"));
    }
    if (failpoint(CommitStage::AfterWalCommitFlush, transaction, true)) {
        return std::unexpected(error(TransactionErrorCode::RecoveryRequired, transaction.id(), "Injected failure after WAL commit flush"));
    }

    auto applied = batch->apply(
        data_directory_,
        *filesystem_,
        false,
        [&](std::size_t, const wal::FileWrite & write) {
            if (failpoint(CommitStage::AfterDeltaApply, transaction, true)) return true;
            return write.mode == wal::FileWriteMode::Truncate &&
                   failpoint(CommitStage::AfterTruncate, transaction, true);
        }
    );
    if (!applied) {
        recovery_required_.store(true, std::memory_order_release);
        transaction.release_writer_guard();
        return std::unexpected(error(TransactionErrorCode::CommittedApplyFailed, transaction.id(), std::move(applied.error().message)));
    }
    if (failpoint(CommitStage::AfterApply, transaction, true)) {
        return std::unexpected(error(TransactionErrorCode::RecoveryRequired, transaction.id(), "Injected failure after participant apply"));
    }
    if (transaction.catalog_snapshot()) {
        auto catalog_restored = catalog_->publish_committed(*transaction.catalog_snapshot());
        if (!catalog_restored) {
            recovery_required_.store(true, std::memory_order_release);
            transaction.release_writer_guard();
            return std::unexpected(error(TransactionErrorCode::CommittedApplyFailed, transaction.id(),
                                          catalog_restored.error().message()));
        }
    }
    auto reloaded = reload_runtime(transaction);
    if (!reloaded) {
        recovery_required_.store(true, std::memory_order_release);
        transaction.release_writer_guard();
        reloaded.error().code = TransactionErrorCode::CommittedApplyFailed;
        return reloaded;
    }
    if (failpoint(CommitStage::AfterRuntimeReload, transaction, true)) {
        return std::unexpected(error(TransactionErrorCode::RecoveryRequired, transaction.id(), "Injected failure after runtime reload"));
    }
    committed_transactions_.fetch_add(1, std::memory_order_relaxed);
    transaction.release_writer_guard();
    commit_succeeded = true;
    return {};
}

std::expected<void, TransactionError> TransactionManager::abort(TransactionContext & transaction)
{
    if (transaction.owner_ != this || !transaction.writer_guard_.owns_lock()) {
        return std::unexpected(error(TransactionErrorCode::InvalidState, transaction.id(),
                                     "Transaction was not started by this TransactionManager"));
    }
    if (transaction.state() == TransactionState::Committed || transaction.state() == TransactionState::Committing) {
        return std::unexpected(error(TransactionErrorCode::InvalidState, transaction.id(), "Committed transaction cannot abort"));
    }
    if (transaction.state() != TransactionState::Aborting && !transaction.transition_to(TransactionState::Aborting)) {
        return std::unexpected(error(TransactionErrorCode::InvalidState, transaction.id(), "Transaction cannot enter abort"));
    }
    if (!transaction.transition_to(TransactionState::Aborted)) {
        return std::unexpected(error(TransactionErrorCode::InvalidState, transaction.id(), "Transaction cannot finish abort"));
    }
    aborted_transactions_.fetch_add(1, std::memory_order_relaxed);
    transaction.release_writer_guard();
    return {};
}

} // namespace litedb::core::transaction

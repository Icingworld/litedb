#include "core/transaction/transaction_manager.hpp"

#include <algorithm>
#include <chrono>
#include <fstream>
#include <limits>
#include <set>
#include <system_error>
#include <utility>

#include "core/schema/schema_loader.hpp"
#include "core/meta/meta_store.hpp"

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
std::expected<void, std::string> copy_directory(
    const std::filesystem::path & source,
    const std::filesystem::path & destination
)
{
    std::error_code error;
    if (!std::filesystem::exists(source, error)) {
        if (error) return std::unexpected("Failed to inspect staging source: " + error.message());
        std::filesystem::create_directories(destination, error);
        if (error) return std::unexpected("Failed to create staging directory: " + error.message());
        return {};
    }
    std::filesystem::create_directories(destination, error);
    if (error) return std::unexpected("Failed to create staging directory: " + error.message());
    for (std::filesystem::recursive_directory_iterator it(source, error), end; it != end; it.increment(error)) {
        if (error) return std::unexpected("Failed to enumerate staging source: " + error.message());
        const auto relative = std::filesystem::relative(it->path(), source, error);
        if (error) return std::unexpected("Failed to resolve staging path: " + error.message());
        const auto target = destination / relative;
        if (it->is_directory()) {
            std::filesystem::create_directories(target, error);
        } else if (it->is_regular_file()) {
            std::filesystem::create_directories(target.parent_path(), error);
            if (!error) std::filesystem::copy_file(it->path(), target, std::filesystem::copy_options::overwrite_existing, error);
        }
        if (error) return std::unexpected("Failed to copy staging data: " + error.message());
    }
    return {};
}

/**
 * @brief 读取文件
 * @param path 文件路径
 * @return 文件内容
 * @details 读取文件内容
 */
std::expected<std::vector<std::byte>, std::string> read_file(const std::filesystem::path & path)
{
    std::ifstream input(path, std::ios::binary | std::ios::ate);
    if (!input) return std::unexpected("Failed to open transaction file: " + path.string());
    const auto end = input.tellg();
    if (end < 0) return std::unexpected("Failed to size transaction file: " + path.string());
    std::vector<std::byte> bytes(static_cast<std::size_t>(end));
    input.seekg(0);
    if (!bytes.empty() && !input.read(reinterpret_cast<char *>(bytes.data()), static_cast<std::streamsize>(bytes.size()))) {
        return std::unexpected("Failed to read transaction file: " + path.string());
    }
    return bytes;
}

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
    meta::MetaEngine & catalog,
    storage::StorageEngine & storage,
    index::IndexEngine & index_engine,
    vindex::VectorIndexEngine & vector_index_engine,
    wal::WalStore & wal,
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
    wal_size_bytes_.store(wal_->size_bytes(), std::memory_order_relaxed);
}

std::expected<std::optional<std::vector<std::byte>>, std::string> read_optional_file(
    const std::filesystem::path & path
)
{
    std::error_code error;
    const auto exists = std::filesystem::exists(path, error);
    if (error) return std::unexpected("Failed to inspect transaction file: " + error.message());
    if (!exists) return std::optional<std::vector<std::byte>> {};
    auto bytes = read_file(path);
    if (!bytes) return std::unexpected(std::move(bytes.error()));
    return std::optional<std::vector<std::byte>> {std::move(*bytes)};
}

std::vector<wal::FileTarget> catalog_physical_targets(const meta::MetaEngine & catalog)
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
    return TransactionMetrics {
        .started_transactions = started_transactions_.load(std::memory_order_relaxed),
        .committed_transactions = committed_transactions_.load(std::memory_order_relaxed),
        .aborted_transactions = aborted_transactions_.load(std::memory_order_relaxed),
        .failed_commits = failed_commits_.load(std::memory_order_relaxed),
        .total_commit_duration_us = total_commit_duration_us_.load(std::memory_order_relaxed),
        .last_commit_duration_us = last_commit_duration_us_.load(std::memory_order_relaxed),
        .maximum_commit_duration_us = maximum_commit_duration_us_.load(std::memory_order_relaxed),
        .wal_size_bytes = wal_size_bytes_.load(std::memory_order_relaxed),
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
    wal_size_bytes_.store(wal_->size_bytes(), std::memory_order_relaxed);
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
    schema::RecordData after
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
    schema::RecordData before,
    schema::RecordData after
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
    schema::RecordData before
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

    std::error_code fs_error;
    std::filesystem::create_directories(staging_directory, fs_error);
    if (fs_error) {
        return std::unexpected(error(TransactionErrorCode::PrepareFailed, transaction.id(),
                                     "Failed to create DDL staging directory: " + fs_error.message()));
    }
    for (const auto * directory : {"collections", "indexes", "vindexes"}) {
        auto copied = copy_directory(data_directory_ / directory, staging_directory / directory);
        if (!copied) {
            return std::unexpected(error(TransactionErrorCode::PrepareFailed, transaction.id(), std::move(copied.error())));
        }
    }

    meta::MetaEngine after_catalog;
    auto restored_catalog = after_catalog.restore(*transaction.catalog_snapshot());
    if (!restored_catalog) {
        return std::unexpected(error(TransactionErrorCode::PrepareFailed, transaction.id(),
                                     mutation_error("meta", std::move(restored_catalog.error().message))));
    }
    meta::MetaStore staged_meta {staging_directory / "meta.lmeta", *filesystem_};
    auto saved_meta = staged_meta.save(*transaction.catalog_snapshot());
    if (!saved_meta) {
        return std::unexpected(error(TransactionErrorCode::PrepareFailed, transaction.id(),
                                     mutation_error("meta", std::move(saved_meta.error().message))));
    }

    storage::StorageEngine staged_storage {staging_directory, *filesystem_};
    for (const auto * database : after_catalog.list_databases()) {
        if (database == nullptr) continue;
        for (const auto * collection : after_catalog.list_collections(database->id())) {
            if (collection == nullptr) continue;
            auto schema = schema::load_collection_schema(after_catalog, collection->id());
            if (!schema) {
                return std::unexpected(error(TransactionErrorCode::PrepareFailed, transaction.id(),
                                             mutation_error("schema", std::move(schema.error().message))));
            }
            auto opened = catalog_->find_collection(collection->id()) != nullptr
                              ? staged_storage.open_collection(std::move(*schema))
                              : staged_storage.create_collection(std::move(*schema));
            if (!opened) {
                return std::unexpected(error(TransactionErrorCode::PrepareFailed, transaction.id(),
                                             mutation_error("storage", std::move(opened.error().message))));
            }
        }
    }

    {
        index::IndexEngine creator {staging_directory, *filesystem_};
        for (const auto * database : after_catalog.list_databases()) {
            if (database == nullptr) continue;
            for (const auto * collection : after_catalog.list_collections(database->id())) {
                if (collection == nullptr) continue;
                auto schema = schema::load_collection_schema(after_catalog, collection->id());
                if (!schema) return std::unexpected(error(TransactionErrorCode::PrepareFailed, transaction.id(), std::move(schema.error().message)));
                for (const auto * index : after_catalog.list_indexes(collection->id())) {
                    if (index == nullptr || catalog_->find_index(index->id()) != nullptr) continue;
                    auto created = creator.create_index(*index, *schema, staged_storage);
                    if (!created) return std::unexpected(error(TransactionErrorCode::PrepareFailed, transaction.id(), mutation_error("scalar index", std::move(created.error().message))));
                }
            }
        }
    }
    index::IndexEngine staged_indexes {staging_directory, *filesystem_};
    auto indexes_restored = staged_indexes.restore_all(after_catalog, staged_storage);
    if (!indexes_restored) {
        return std::unexpected(error(TransactionErrorCode::PrepareFailed, transaction.id(),
                                     mutation_error("scalar index", std::move(indexes_restored.error().message))));
    }

    {
        vindex::VectorIndexEngine creator {staging_directory / "vindexes", *filesystem_};
        for (const auto * database : after_catalog.list_databases()) {
            if (database == nullptr) continue;
            for (const auto * collection : after_catalog.list_collections(database->id())) {
                if (collection == nullptr) continue;
                auto schema = schema::load_collection_schema(after_catalog, collection->id());
                if (!schema) return std::unexpected(error(TransactionErrorCode::PrepareFailed, transaction.id(), std::move(schema.error().message)));
                for (const auto * index : after_catalog.list_vector_indexes(collection->id())) {
                    if (index == nullptr || catalog_->find_vector_index(index->id()) != nullptr) continue;
                    auto created = creator.create_index(*index, *schema, staged_storage);
                    if (!created) return std::unexpected(error(TransactionErrorCode::PrepareFailed, transaction.id(), mutation_error("vector index", std::move(created.error().message))));
                }
            }
        }
    }
    vindex::VectorIndexEngine staged_vectors {staging_directory / "vindexes", *filesystem_};
    auto vectors_restored = staged_vectors.restore_all(after_catalog, staged_storage);
    if (!vectors_restored) {
        return std::unexpected(error(TransactionErrorCode::PrepareFailed, transaction.id(),
                                     mutation_error("vector index", std::move(vectors_restored.error().message))));
    }

    const auto before_targets = catalog_physical_targets(*catalog_);
    const auto after_targets = catalog_physical_targets(after_catalog);
    wal::FileWriteBatch batch;
    auto add_replacement = [&](wal::FileTarget target) -> std::expected<void, TransactionError> {
        auto live = read_optional_file(wal::FileWriteBatch::resolve_target(data_directory_, target));
        if (!live) return std::unexpected(error(TransactionErrorCode::PrepareFailed, transaction.id(), std::move(live.error())));
        auto staged = read_file(wal::FileWriteBatch::resolve_target(staging_directory, target));
        if (!staged) return std::unexpected(error(TransactionErrorCode::PrepareFailed, transaction.id(), std::move(staged.error())));
        if (!*live || **live != *staged) {
            batch.add(wal::FileWrite {
                .target = target,
                .offset = 0,
                .after_image = std::move(*staged),
                .mode = wal::FileWriteMode::Replace,
            });
        }
        return {};
    };

    for (const auto & target : after_targets) {
        auto added = add_replacement(target);
        if (!added) return std::unexpected(std::move(added.error()));
    }
    auto meta_added = add_replacement({.kind = wal::FileKind::MetaStore, .object_id = 0});
    if (!meta_added) return std::unexpected(std::move(meta_added.error()));
    for (const auto & target : before_targets) {
        if (!contains_target(after_targets, target)) {
            batch.add(wal::FileWrite {
                .target = target,
                .offset = 0,
                .after_image = {},
                .mode = wal::FileWriteMode::Delete,
            });
        }
    }
    return batch;
}

std::expected<wal::FileWriteBatch, TransactionError> TransactionManager::prepare(
    const TransactionContext & transaction,
    const std::filesystem::path & staging_directory
)
{
    if (transaction.catalog_snapshot()) {
        return prepare_catalog(transaction, staging_directory);
    }
    std::error_code fs_error;
    std::filesystem::create_directories(staging_directory, fs_error);
    if (fs_error) {
        return std::unexpected(error(TransactionErrorCode::PrepareFailed, transaction.id(),
                                     "Failed to create transaction staging directory: " + fs_error.message()));
    }
    for (const auto * directory : {"collections", "indexes", "vindexes"}) {
        auto copied = copy_directory(data_directory_ / directory, staging_directory / directory);
        if (!copied) {
            return std::unexpected(error(TransactionErrorCode::PrepareFailed, transaction.id(), std::move(copied.error())));
        }
    }

    {
        storage::StorageEngine staged_storage {staging_directory, *filesystem_};
        for (const auto * database : catalog_->list_databases()) {
            if (database == nullptr) continue;
            for (const auto * collection : catalog_->list_collections(database->id())) {
                if (collection == nullptr) continue;
                auto schema = schema::load_collection_schema(*catalog_, collection->id());
                if (!schema) {
                    return std::unexpected(error(TransactionErrorCode::PrepareFailed, transaction.id(),
                                                 mutation_error("schema", std::move(schema.error().message))));
                }
                auto opened = staged_storage.open_collection(std::move(*schema));
                if (!opened) {
                    return std::unexpected(error(TransactionErrorCode::PrepareFailed, transaction.id(),
                                                 mutation_error("storage", std::move(opened.error().message))));
                }
            }
        }
        index::IndexEngine staged_indexes {staging_directory, *filesystem_};
        auto indexes_restored = staged_indexes.restore_all(*catalog_, staged_storage);
        if (!indexes_restored) {
            return std::unexpected(error(TransactionErrorCode::PrepareFailed, transaction.id(),
                                         mutation_error("scalar index", std::move(indexes_restored.error().message))));
        }
        vindex::VectorIndexEngine staged_vectors {staging_directory / "vindexes", *filesystem_};
        auto vectors_restored = staged_vectors.restore_all(*catalog_, staged_storage);
        if (!vectors_restored) {
            return std::unexpected(error(TransactionErrorCode::PrepareFailed, transaction.id(),
                                         mutation_error("vector index", std::move(vectors_restored.error().message))));
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
                if (!inserted) return std::unexpected(error(TransactionErrorCode::PrepareFailed, transaction.id(), mutation_error("storage", std::move(inserted.error().message))));
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
                if (!stored) return std::unexpected(error(TransactionErrorCode::PrepareFailed, transaction.id(), mutation_error("storage", std::move(stored.error().message))));
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
                if (!stored) return std::unexpected(error(TransactionErrorCode::PrepareFailed, transaction.id(), mutation_error("storage", std::move(stored.error().message))));
                break;
            }
            }
        }
    }

    std::set<common::CollectionId> collections;
    for (const auto & mutation : transaction.write_set()) collections.insert(mutation.collection_id);

    wal::FileWriteBatch batch;
    auto add_changed = [&](wal::FileTarget target) -> std::expected<void, TransactionError> {
        const auto live_path = wal::FileWriteBatch::resolve_target(data_directory_, target);
        const auto staged_path = wal::FileWriteBatch::resolve_target(staging_directory, target);
        auto live = read_file(live_path);
        if (!live) return std::unexpected(error(TransactionErrorCode::PrepareFailed, transaction.id(), std::move(live.error())));
        auto staged = read_file(staged_path);
        if (!staged) return std::unexpected(error(TransactionErrorCode::PrepareFailed, transaction.id(), std::move(staged.error())));
        if (*live != *staged) {
            batch.add(wal::FileWrite {
                .target = target,
                .offset = 0,
                .after_image = std::move(*staged),
                .mode = wal::FileWriteMode::Replace,
            });
        }
        return {};
    };

    for (const auto collection_id : collections) {
        auto added = add_changed({.kind = wal::FileKind::CollectionStore, .object_id = collection_id});
        if (!added) return std::unexpected(std::move(added.error()));
        for (const auto & index : index_engine_->list_indexes(collection_id)) {
            added = add_changed({.kind = wal::FileKind::ScalarIndex, .object_id = index.index_id});
            if (!added) return std::unexpected(std::move(added.error()));
        }
        for (const auto & index : vector_index_engine_->list_indexes(collection_id)) {
            if (index.kind != vindex::VectorIndexKind::Hnsw) continue;
            added = add_changed({.kind = wal::FileKind::VectorIndex, .object_id = index.index_id});
            if (!added) return std::unexpected(std::move(added.error()));
        }
    }
    return batch;
}

std::expected<void, TransactionError> TransactionManager::reload_runtime(TransactionId transaction_id)
{
    storage::StorageEngine restored_storage {data_directory_, *filesystem_};
    for (const auto * database : catalog_->list_databases()) {
        if (database == nullptr) continue;
        for (const auto * collection : catalog_->list_collections(database->id())) {
            if (collection == nullptr) continue;
            auto schema = schema::load_collection_schema(*catalog_, collection->id());
            if (!schema) return std::unexpected(error(TransactionErrorCode::ApplyFailed, transaction_id, std::move(schema.error().message)));
            auto opened = restored_storage.open_collection(std::move(*schema));
            if (!opened) return std::unexpected(error(TransactionErrorCode::ApplyFailed, transaction_id, std::move(opened.error().message)));
        }
    }
    index::IndexEngine restored_indexes {data_directory_, *filesystem_};
    auto indexes = restored_indexes.restore_all(*catalog_, restored_storage);
    if (!indexes) return std::unexpected(error(TransactionErrorCode::ApplyFailed, transaction_id, std::move(indexes.error().message)));
    *storage_ = std::move(restored_storage);
    *index_engine_ = std::move(restored_indexes);

    // FlatIndex keeps a non-owning StorageEngine pointer, so construct the
    // replacement vector engine only after the live storage object is published.
    vindex::VectorIndexEngine restored_vectors {data_directory_ / "vindexes", *filesystem_};
    auto vectors = restored_vectors.restore_all(*catalog_, *storage_);
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

    auto applied = batch->apply(data_directory_, *filesystem_, false);
    if (!applied) {
        recovery_required_.store(true, std::memory_order_release);
        transaction.release_writer_guard();
        return std::unexpected(error(TransactionErrorCode::CommittedApplyFailed, transaction.id(), std::move(applied.error().message)));
    }
    if (failpoint(CommitStage::AfterApply, transaction, true)) {
        return std::unexpected(error(TransactionErrorCode::RecoveryRequired, transaction.id(), "Injected failure after participant apply"));
    }
    if (transaction.catalog_snapshot()) {
        auto catalog_restored = catalog_->restore(*transaction.catalog_snapshot());
        if (!catalog_restored) {
            recovery_required_.store(true, std::memory_order_release);
            transaction.release_writer_guard();
            return std::unexpected(error(TransactionErrorCode::CommittedApplyFailed, transaction.id(),
                                         std::move(catalog_restored.error().message)));
        }
    }
    auto reloaded = reload_runtime(transaction.id());
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

#include "core/catalog/catalog_editor.hpp"
#include "core/catalog/catalog_publisher.hpp"
#include "core/filesystem/backend/file_handle_backend.hpp"
#include "core/filesystem/backend/filesystem_backend.hpp"
#include "core/filesystem/backend/platform_filesystem_backend.hpp"
#include "core/filesystem/platform_filesystem.hpp"
#include "core/index/index_engine.hpp"
#include "core/storage/storage_engine.hpp"
#include "core/transaction/transaction_manager.hpp"
#include "core/vindex/vector_index_engine.hpp"
#include "core/wal/wal_codec.hpp"
#include "core/wal/wal_manager.hpp"
#include "core/wal/wal_store.hpp"

#include <algorithm>
#include <cstddef>
#include <filesystem>
#include <memory>
#include <optional>
#include <stdexcept>
#include <string>
#include <utility>
#include <vector>

namespace
{

using namespace litedb::core;

struct FaultPlan
{
    std::size_t append_failure_call {0};
    std::size_t append_calls {0};
    std::size_t partial_bytes {1};
    std::optional<wal::WalRecordType> append_failure_type;
    bool fail_truncate {false};
    bool fail_sync_data {false};
};

error::Error fault_error(const std::filesystem::path & path, const char * operation)
{
    return error::Error {
        filesystem::FileSystemErrorCode::IoError,
        std::string("fault injected in ") + operation,
        filesystem::FileSystemErrorContext {
            operation,
            path,
            {},
            {},
        },
    };
}

class FaultFileHandleBackend final : public filesystem::backend::FileHandleBackend
{
public:
    FaultFileHandleBackend(
        std::filesystem::path path,
        std::unique_ptr<filesystem::backend::FileHandleBackend> inner,
        std::shared_ptr<FaultPlan> plan
    )
        : path_(std::move(path))
        , inner_(std::move(inner))
        , plan_(std::move(plan))
    {}

    std::expected<void, filesystem::FileSystemError> close() override
    {
        return inner_->close();
    }

    std::expected<std::size_t, filesystem::FileSystemError>
    read_at(std::uint64_t offset, std::span<std::byte> buffer) override
    {
        return inner_->read_at(offset, buffer);
    }

    std::expected<void, filesystem::FileSystemError>
    write_at(std::uint64_t offset, std::span<const std::byte> data) override
    {
        return inner_->write_at(offset, data);
    }

    std::expected<void, filesystem::FileSystemError> append(
        std::span<const std::byte> data
    ) override
    {
        ++plan_->append_calls;
        const auto record_type =
            data.size() > 6 ? std::optional {std::to_integer<std::uint8_t>(data[6])} : std::nullopt;
        const auto type_matches =
            plan_->append_failure_type.has_value() && record_type.has_value() &&
            *record_type == static_cast<std::uint8_t>(*plan_->append_failure_type);
        if ((plan_->append_failure_call != 0 &&
             plan_->append_calls == plan_->append_failure_call) ||
            type_matches) {
            auto size = inner_->size();
            if (!size) [[unlikely]] {
                return std::unexpected(std::move(size.error()));
            }
            const auto partial_size = std::min(plan_->partial_bytes, data.size());
            if (partial_size != 0) {
                auto partial = inner_->write_at(*size, data.first(partial_size));
                if (!partial) [[unlikely]] {
                    return std::unexpected(std::move(partial.error()));
                }
            }
            plan_->append_failure_call = 0;
            plan_->append_failure_type.reset();
            return std::unexpected(fault_error(path_, "append"));
        }
        return inner_->append(data);
    }

    std::expected<std::uint64_t, filesystem::FileSystemError> size() override
    {
        return inner_->size();
    }

    std::expected<void, filesystem::FileSystemError> truncate(std::uint64_t size) override
    {
        if (plan_->fail_truncate) {
            plan_->fail_truncate = false;
            return std::unexpected(fault_error(path_, "truncate"));
        }
        return inner_->truncate(size);
    }

    std::expected<void, filesystem::FileSystemError> sync_data() override
    {
        if (plan_->fail_sync_data) {
            plan_->fail_sync_data = false;
            return std::unexpected(fault_error(path_, "sync_data"));
        }
        return inner_->sync_data();
    }

    std::expected<void, filesystem::FileSystemError> sync_all() override
    {
        return inner_->sync_all();
    }

private:
    std::filesystem::path path_;
    std::unique_ptr<filesystem::backend::FileHandleBackend> inner_;
    std::shared_ptr<FaultPlan> plan_;
};

class FaultFileSystemBackend final : public filesystem::backend::FileSystemBackend
{
public:
    explicit FaultFileSystemBackend(std::shared_ptr<FaultPlan> plan)
        : inner_(filesystem::backend::create_platform_filesystem_backend())
        , plan_(std::move(plan))
    {}

    std::expected<
        std::unique_ptr<filesystem::backend::FileHandleBackend>,
        filesystem::FileSystemError>
    open(const std::filesystem::path & path, const filesystem::FileOpenOptions & options) override
    {
        auto opened = inner_->open(path, options);
        if (!opened) [[unlikely]] {
            return std::unexpected(std::move(opened.error()));
        }
        return std::make_unique<FaultFileHandleBackend>(path, std::move(*opened), plan_);
    }

    std::expected<std::vector<std::filesystem::path>, filesystem::FileSystemError> list_dir(
        const std::filesystem::path & path
    ) override
    {
        return inner_->list_dir(path);
    }

    std::expected<bool, filesystem::FileSystemError> exists(
        const std::filesystem::path & path
    ) override
    {
        return inner_->exists(path);
    }

    std::expected<void, filesystem::FileSystemError> create_dir_all(
        const std::filesystem::path & path
    ) override
    {
        return inner_->create_dir_all(path);
    }

    std::expected<void, filesystem::FileSystemError>
    rename(const std::filesystem::path & from, const std::filesystem::path & to) override
    {
        return inner_->rename(from, to);
    }

    std::expected<void, filesystem::FileSystemError> replace_file_atomic(
        const std::filesystem::path & from,
        const std::filesystem::path & to
    ) override
    {
        return inner_->replace_file_atomic(from, to);
    }

    std::expected<void, filesystem::FileSystemError> remove(
        const std::filesystem::path & path
    ) override
    {
        return inner_->remove(path);
    }

    std::expected<void, filesystem::FileSystemError> sync_directory(
        const std::filesystem::path & path
    ) override
    {
        return inner_->sync_directory(path);
    }

private:
    std::unique_ptr<filesystem::backend::FileSystemBackend> inner_;
    std::shared_ptr<FaultPlan> plan_;
};

filesystem::FileSystem make_fault_filesystem(const std::shared_ptr<FaultPlan> & plan)
{
    return filesystem::FileSystem {std::make_unique<FaultFileSystemBackend>(plan)};
}

void require(bool condition, const char * message)
{
    if (!condition) {
        throw std::runtime_error(message);
    }
}

std::filesystem::path temp_dir(const char * name)
{
    auto path = std::filesystem::temp_directory_path() / name;
    std::filesystem::remove_all(path);
    std::filesystem::create_directories(path);
    return path;
}

// 通过真实事务提交路径验证 WAL 追加和提交刷盘错误的状态传播。
void run_transaction_wal_fault(
    const char * directory_name,
    std::optional<wal::WalRecordType> append_failure_type,
    bool fail_truncate,
    bool fail_sync_data,
    bool expect_recovery
)
{
    auto plan = std::make_shared<FaultPlan>();
    auto filesystem = make_fault_filesystem(plan);
    const auto directory = temp_dir(directory_name);
    catalog::CatalogPublisher catalog {directory / "catalog.lcat", filesystem};
    require(catalog.open_or_initialize().has_value(), "fault transaction catalog open failed");
    storage::StorageEngine storage {
        directory,
        filesystem,
        storage::StorageOpenMode::TransactionalStaging,
    };
    index::IndexEngine indexes {directory, filesystem};
    vindex::VectorIndexEngine vectors {directory / "vindexes", filesystem};
    auto wal = wal::WalManager::open(directory / "wal", filesystem);
    require(wal.has_value(), "fault transaction WAL open failed");
    transaction::TransactionManager manager {
        directory,
        filesystem,
        catalog,
        storage,
        indexes,
        vectors,
        *wal,
        0,
    };

    auto transaction = manager.begin_implicit();
    require(transaction.has_value(), "fault transaction begin failed");
    auto editor = catalog::CatalogEditor::from(catalog.snapshot());
    require(editor.has_value(), "fault transaction catalog editor failed");
    require(
        editor->create_database(catalog::CreateDatabaseRequest {.database_name = "fault_db"})
            .has_value(),
        "fault transaction catalog mutation failed"
    );
    require(
        manager.stage_catalog(*transaction, editor->snapshot()).has_value(),
        "fault transaction staging failed"
    );
    plan->append_failure_type = append_failure_type;
    plan->fail_truncate = fail_truncate;
    plan->fail_sync_data = fail_sync_data;

    auto committed = manager.commit(*transaction);
    if (committed || !committed.error().is(filesystem::FileSystemErrorCode::IoError)) {
        throw std::runtime_error(
            std::string(directory_name) +
            " transaction did not preserve the injected WAL filesystem error: " +
            (committed ? "commit unexpectedly succeeded" : committed.error().message())
        );
    }
    require(
        manager.recovery_required() == expect_recovery,
        "transaction manager recovery state did not match WAL certainty"
    );
    if (expect_recovery) {
        auto refused = manager.begin_implicit();
        require(
            !refused && refused.error().is(transaction::TransactionErrorCode::RecoveryRequired),
            "transaction manager accepted work after WAL certainty was lost"
        );
    } else {
        auto next = manager.begin_implicit();
        require(next.has_value(), "transaction manager rejected work after a confirmed rollback");
        require(manager.abort(*next).has_value(), "post-rollback transaction abort failed");
    }
}

} // namespace

int main()
{
    const auto write = wal::FileWrite {
        .target = {.kind = wal::FileKind::CollectionStore, .object_id = 3},
        .offset = 0,
        .after_image = {std::byte {1}},
    };

    {
        auto plan = std::make_shared<FaultPlan>();
        auto filesystem = make_fault_filesystem(plan);
        auto store = wal::WalStore::create(
            temp_dir("litedb_wal_fault_rollback") / "wal" / "segment.wal",
            filesystem,
            wal::WalFileHeader {}
        );
        require(store.has_value(), "fault test store create failed");
        plan->append_failure_call = 1;
        plan->partial_bytes = 20;
        auto failed = store->append_begin(1);
        require(
            !failed && failed.error().is(filesystem::FileSystemErrorCode::IoError),
            "append failure did not preserve the original error"
        );
        require(
            !store->recovery_required() && store->size_bytes() == wal::WalCodec::FileHeaderSize,
            "successful append rollback did not preserve the old tail"
        );
        require(
            store->append_begin(1).has_value(),
            "store was not writable after successful rollback"
        );
        require(store->scan(false).has_value(), "scan after successful rollback failed");
    }

    {
        auto plan = std::make_shared<FaultPlan>();
        auto filesystem = make_fault_filesystem(plan);
        auto store = wal::WalStore::create(
            temp_dir("litedb_wal_fault_file_write") / "wal" / "segment.wal",
            filesystem,
            wal::WalFileHeader {}
        );
        require(store.has_value(), "file-write fault store create failed");
        require(store->append_begin(1).has_value(), "file-write fault begin failed");
        plan->append_failure_call = 2;
        plan->partial_bytes = 7;
        auto failed = store->append_write(1, write);
        require(
            !failed && failed.error().is(filesystem::FileSystemErrorCode::IoError),
            "file-write append failure did not preserve the original error"
        );
        require(!store->recovery_required(), "file-write rollback unexpectedly poisoned the store");
        require(
            store->append_commit(1).has_value(),
            "store was not writable after file-write rollback"
        );
    }

    {
        auto plan = std::make_shared<FaultPlan>();
        auto filesystem = make_fault_filesystem(plan);
        auto store = wal::WalStore::create(
            temp_dir("litedb_wal_fault_commit") / "wal" / "segment.wal",
            filesystem,
            wal::WalFileHeader {}
        );
        require(store.has_value(), "commit fault store create failed");
        require(store->append_begin(1).has_value(), "commit fault begin failed");
        require(store->append_write(1, write).has_value(), "commit fault file-write failed");
        plan->append_failure_call = 3;
        plan->partial_bytes = 13;
        auto failed = store->append_commit(1);
        require(
            !failed && failed.error().is(filesystem::FileSystemErrorCode::IoError),
            "commit append failure did not preserve the original error"
        );
        require(!store->recovery_required(), "commit rollback unexpectedly poisoned the store");
        require(store->append_begin(2).has_value(), "store was not writable after commit rollback");
    }

    {
        auto plan = std::make_shared<FaultPlan>();
        auto filesystem = make_fault_filesystem(plan);
        auto store = wal::WalStore::create(
            temp_dir("litedb_wal_fault_truncate") / "wal" / "segment.wal",
            filesystem,
            wal::WalFileHeader {}
        );
        require(store.has_value(), "truncate fault store create failed");
        plan->append_failure_call = 1;
        plan->fail_truncate = true;
        auto failed = store->append_begin(1);
        require(
            !failed && failed.error().is(filesystem::FileSystemErrorCode::IoError),
            "truncate failure did not preserve the rollback error"
        );
        require(store->recovery_required(), "truncate rollback failure did not poison the store");
        auto refused = store->append_begin(1);
        require(
            !refused && refused.error().is(wal::WalErrorCode::RecoveryRequired),
            "poisoned store accepted an append"
        );
        require(store->scan(false).has_value(), "poisoned store did not allow read-only scan");
    }

    {
        auto plan = std::make_shared<FaultPlan>();
        auto filesystem = make_fault_filesystem(plan);
        auto store = wal::WalStore::create(
            temp_dir("litedb_wal_fault_sync") / "wal" / "segment.wal",
            filesystem,
            wal::WalFileHeader {}
        );
        require(store.has_value(), "sync fault store create failed");
        plan->append_failure_call = 1;
        plan->fail_sync_data = true;
        auto failed = store->append_begin(1);
        require(
            !failed && failed.error().is(filesystem::FileSystemErrorCode::IoError),
            "rollback sync failure did not preserve the sync error"
        );
        require(store->recovery_required(), "rollback sync failure did not poison the store");
    }

    {
        auto plan = std::make_shared<FaultPlan>();
        auto filesystem = make_fault_filesystem(plan);
        auto store = wal::WalStore::create(
            temp_dir("litedb_wal_fault_flush") / "wal" / "segment.wal",
            filesystem,
            wal::WalFileHeader {}
        );
        require(store.has_value(), "flush fault store create failed");
        auto begin = store->append_begin(1);
        require(begin.has_value(), "flush fault begin failed");
        plan->fail_sync_data = true;
        auto failed = store->flush_through(*begin);
        require(
            !failed && failed.error().is(filesystem::FileSystemErrorCode::IoError),
            "flush failure did not preserve the underlying error"
        );
        require(store->recovery_required(), "flush failure did not poison the store");
        auto refused = store->append_begin(2);
        require(
            !refused && refused.error().is(wal::WalErrorCode::RecoveryRequired),
            "store accepted writes after flush persistence became unknown"
        );
    }

    {
        auto plan = std::make_shared<FaultPlan>();
        auto filesystem = make_fault_filesystem(plan);
        const auto directory = temp_dir("litedb_wal_manager_fault");
        auto manager = wal::WalManager::open(directory / "wal", filesystem);
        require(manager.has_value(), "fault manager open failed");
        plan->append_failure_call = 1;
        plan->fail_truncate = true;
        auto failed = manager->append_write(1, write);
        require(
            !failed && manager->recovery_required(),
            "manager did not expose poisoned active segment"
        );
        auto refused = manager->append_begin(2);
        require(
            !refused && refused.error().is(wal::WalErrorCode::RecoveryRequired),
            "manager accepted writes after poisoning"
        );
    }

    run_transaction_wal_fault(
        "litedb_wal_fault_txn_begin",
        wal::WalRecordType::Begin,
        false,
        false,
        false
    );
    run_transaction_wal_fault(
        "litedb_wal_fault_txn_write",
        wal::WalRecordType::FileWrite,
        false,
        false,
        false
    );
    run_transaction_wal_fault(
        "litedb_wal_fault_txn_commit",
        wal::WalRecordType::Commit,
        false,
        false,
        false
    );
    run_transaction_wal_fault(
        "litedb_wal_fault_txn_rollback",
        wal::WalRecordType::Begin,
        true,
        false,
        true
    );
    run_transaction_wal_fault("litedb_wal_fault_txn_flush", std::nullopt, false, true, true);

    return 0;
}

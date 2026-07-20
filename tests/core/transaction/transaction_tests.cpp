#include "core/transaction/transaction_context.hpp"
#include "core/transaction/transaction_manager.hpp"
#include "core/filesystem/platform_filesystem.hpp"

#include <chrono>
#include <filesystem>
#include <future>
#include <stdexcept>
#include <type_traits>

namespace
{
using namespace litedb::core::transaction;

void require(bool condition, const char * message)
{
    if (!condition) throw std::runtime_error(message);
}
}

int main()
{
    static_assert(!std::is_copy_constructible_v<TransactionContext>);
    static_assert(std::is_move_constructible_v<TransactionContext>);
    require(can_transition(TransactionState::Active, TransactionState::Preparing), "active should prepare");
    require(can_transition(TransactionState::Preparing, TransactionState::Committing), "prepare should commit");
    require(can_transition(TransactionState::Committing, TransactionState::Committed), "commit should finish");
    require(!can_transition(TransactionState::Committed, TransactionState::Aborting), "committed cannot abort");
    require(can_transition(TransactionState::Active, TransactionState::Aborting), "active should abort");
    require(can_transition(TransactionState::Aborting, TransactionState::Aborted), "abort should finish");
    TransactionContext context {1};
    require(context.id() == 1 && context.state() == TransactionState::Active, "context defaults mismatch");

    const auto directory = std::filesystem::temp_directory_path() / "litedb_transaction_single_writer";
    std::filesystem::remove_all(directory);
    std::filesystem::create_directories(directory);
    auto filesystem = litedb::core::filesystem::create_platform_filesystem();
    litedb::core::meta::MetaEngine catalog;
    litedb::core::storage::StorageEngine storage {directory, filesystem};
    litedb::core::index::IndexEngine indexes {directory, filesystem};
    litedb::core::vindex::VectorIndexEngine vectors {directory / "vindexes", filesystem};
    auto wal = litedb::core::wal::WalStore::open(directory / "wal" / "litedb.wal", filesystem);
    require(wal.has_value(), "single-writer WAL open failed");
    TransactionManager manager {directory, filesystem, catalog, storage, indexes, vectors, *wal, 0};

    auto first = manager.begin_implicit();
    require(first.has_value(), "first transaction begin failed");
    auto second = std::async(std::launch::async, [&manager] {
        auto transaction = manager.begin_implicit();
        if (!transaction) return false;
        return manager.abort(*transaction).has_value();
    });
    require(second.wait_for(std::chrono::milliseconds {100}) == std::future_status::timeout,
            "second writer was not serialized by TransactionManager");
    require(manager.abort(*first).has_value(), "first transaction abort failed");
    require(second.wait_for(std::chrono::seconds {5}) == std::future_status::ready && second.get(),
            "second writer did not resume after abort");
    const auto metrics = manager.metrics();
    require(metrics.started_transactions == 2 && metrics.aborted_transactions == 2,
            "single-writer metrics mismatch");
    require(metrics.wal_size_bytes >= litedb::core::wal::WalCodec::FileHeaderSize,
            "WAL size observation mismatch");
    return 0;
}

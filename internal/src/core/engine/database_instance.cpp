#include "core/engine/database_instance.hpp"

#include <stdexcept>

namespace litedb::core::engine
{

DatabaseInstance::DatabaseInstance(DatabaseConfig config)
{
    if (config.data_dir.has_value()) {
        persistence_ = std::make_unique<persistence::PersistenceController>(
            config.data_dir.value(),
            meta_,
            storage_,
            index_manager_
        );
        auto initialized = persistence_->initialize();
        if (!initialized.has_value()) {
            throw std::runtime_error(initialized.error().message);
        }
    }
}

meta::MetaEngine & DatabaseInstance::meta() noexcept
{
    return meta_;
}

const meta::MetaEngine & DatabaseInstance::meta() const noexcept
{
    return meta_;
}

storage::StorageManager & DatabaseInstance::storage() noexcept
{
    return storage_;
}

const storage::StorageManager & DatabaseInstance::storage() const noexcept
{
    return storage_;
}

index::IndexManager & DatabaseInstance::index_manager() noexcept
{
    return index_manager_;
}

const index::IndexManager & DatabaseInstance::index_manager() const noexcept
{
    return index_manager_;
}

std::mutex & DatabaseInstance::mutex() noexcept
{
    return mutex_;
}

executor::DdlMutationHandler * DatabaseInstance::ddl_handler() noexcept
{
    return persistence_.get();
}

} // namespace litedb::core::engine

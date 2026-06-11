#include "core/engine/database_instance.hpp"

#include <stdexcept>

namespace litedb::core::engine
{

DatabaseInstance::DatabaseInstance(DatabaseConfig config)
{
    if (config.data_dir.has_value()) {
        persistence_ = std::make_unique<persistence::PersistenceController>(
            config.data_dir.value(),
            catalog_,
            storage_
        );
        auto initialized = persistence_->initialize();
        if (!initialized.has_value()) {
            throw std::runtime_error(initialized.error().message);
        }
    }
}

catalog::InMemoryCatalog & DatabaseInstance::catalog() noexcept
{
    return catalog_;
}

const catalog::InMemoryCatalog & DatabaseInstance::catalog() const noexcept
{
    return catalog_;
}

storage::StorageManager & DatabaseInstance::storage() noexcept
{
    return storage_;
}

const storage::StorageManager & DatabaseInstance::storage() const noexcept
{
    return storage_;
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

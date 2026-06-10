#include "core/engine/database_instance.hpp"

namespace litedb::core::engine
{

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

} // namespace litedb::core::engine

#include "core/engine/engine.hpp"

namespace litedb::core::engine
{

Engine::Engine()
    : instance_()
    , session_(instance_)
{
}

std::expected<executor::ExecutionResult, EngineError> Engine::execute_sql(std::string_view sql)
{
    return session_.execute_sql(sql);
}

std::optional<common::DatabaseId> Engine::current_database_id() const noexcept
{
    return session_.current_database_id();
}

catalog::InMemoryCatalog & Engine::catalog() noexcept
{
    return instance_.catalog();
}

const catalog::InMemoryCatalog & Engine::catalog() const noexcept
{
    return instance_.catalog();
}

storage::StorageManager & Engine::storage() noexcept
{
    return instance_.storage();
}

const storage::StorageManager & Engine::storage() const noexcept
{
    return instance_.storage();
}

index::IndexManager & Engine::index_manager() noexcept
{
    return instance_.index_manager();
}

const index::IndexManager & Engine::index_manager() const noexcept
{
    return instance_.index_manager();
}

} // namespace litedb::core::engine

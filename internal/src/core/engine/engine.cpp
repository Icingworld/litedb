#include "core/engine/engine.hpp"

#include <utility>

namespace litedb::core::engine
{

Engine::Engine(DatabaseConfig config)
    : instance_(std::move(config))
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

meta::MetaEngine & Engine::meta() noexcept
{
    return instance_.meta();
}

const meta::MetaEngine & Engine::meta() const noexcept
{
    return instance_.meta();
}

storage::StorageEngine & Engine::storage() noexcept
{
    return instance_.storage();
}

const storage::StorageEngine & Engine::storage() const noexcept
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

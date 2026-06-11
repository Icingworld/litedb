#pragma once

#include <filesystem>
#include <memory>
#include <mutex>
#include <optional>

#include "core/catalog/in_memory_catalog.hpp"
#include "core/executor/executor.hpp"
#include "core/persistence/persistence_controller.hpp"
#include "core/storage/storage_manager.hpp"

namespace litedb::core::engine
{

/**
 * @brief 数据库配置
 */
struct DatabaseConfig
{
    std::optional<std::filesystem::path> data_dir;      ///< 数据目录
};

/**
 * @brief 数据库实例
 */
class DatabaseInstance
{
public:
    DatabaseInstance() = default;

    explicit DatabaseInstance(DatabaseConfig config);

    DatabaseInstance(const DatabaseInstance &) = delete;

    DatabaseInstance & operator=(const DatabaseInstance &) = delete;

public:
    [[nodiscard]]
    catalog::InMemoryCatalog & catalog() noexcept;

    [[nodiscard]]
    const catalog::InMemoryCatalog & catalog() const noexcept;

    [[nodiscard]]
    storage::StorageManager & storage() noexcept;

    [[nodiscard]]
    const storage::StorageManager & storage() const noexcept;

    [[nodiscard]]
    std::mutex & mutex() noexcept;

    [[nodiscard]]
    executor::DdlMutationHandler * ddl_handler() noexcept;

private:
    catalog::InMemoryCatalog catalog_;                                      ///< 目录
    storage::StorageManager storage_;                                       ///< 存储管理器
    std::unique_ptr<persistence::PersistenceController> persistence_;       ///< 持久化控制器
    std::mutex mutex_;                                                      ///< 互斥锁
};

} // namespace litedb::core::engine

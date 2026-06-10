#pragma once

#include <mutex>

#include "core/catalog/in_memory_catalog.hpp"
#include "core/storage/storage_manager.hpp"

namespace litedb::core::engine
{

/**
 * @brief 数据库实例
 * @details 目前的实现中，所有 session 共享同一个数据库实例
 */
class DatabaseInstance
{
public:
    DatabaseInstance() = default;

    DatabaseInstance(const DatabaseInstance &) = delete;

    DatabaseInstance & operator=(const DatabaseInstance &) = delete;

public:
    /**
     * @brief 获取目录
     * @return 目录
     */
    [[nodiscard]]
    catalog::InMemoryCatalog & catalog() noexcept;

    /**
     * @brief 获取目录
     * @return 目录
     */
    [[nodiscard]]
    const catalog::InMemoryCatalog & catalog() const noexcept;

    /**
     * @brief 获取存储管理器
     * @return 存储管理器
     */
    [[nodiscard]]
    storage::StorageManager & storage() noexcept;

    /**
     * @brief 获取存储管理器
     * @return 存储管理器
     */
    [[nodiscard]]
    const storage::StorageManager & storage() const noexcept;

    /**
     * @brief 获取互斥锁
     * @return 互斥锁
     */
    [[nodiscard]]
    std::mutex & mutex() noexcept;

private:
    catalog::InMemoryCatalog catalog_;      ///< 目录
    storage::StorageManager storage_;       ///< 存储管理器
    std::mutex mutex_;                      ///< 互斥锁
};

} // namespace litedb::core::engine

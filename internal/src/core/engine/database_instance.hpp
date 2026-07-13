#pragma once

#include <filesystem>
#include <memory>
#include <mutex>

#include "core/meta/meta_engine.hpp"
#include "core/executor/executor.hpp"
#include "core/index/index_manager.hpp"
#include "core/persistence/persistence_controller.hpp"
#include "core/storage/storage_engine.hpp"

namespace litedb::core::engine
{

/**
 * @brief 数据库配置
 */
struct DatabaseConfig
{
    std::filesystem::path data_dir;      ///< 数据目录
};

/**
 * @brief 数据库实例
 */
class DatabaseInstance
{
public:
    explicit DatabaseInstance(DatabaseConfig config);

    DatabaseInstance(const DatabaseInstance &) = delete;

    DatabaseInstance & operator=(const DatabaseInstance &) = delete;

public:
    [[nodiscard]]
    meta::MetaEngine & meta() noexcept;

    [[nodiscard]]
    const meta::MetaEngine & meta() const noexcept;

    [[nodiscard]]
    storage::StorageEngine & storage() noexcept;

    [[nodiscard]]
    const storage::StorageEngine & storage() const noexcept;

    [[nodiscard]]
    index::IndexManager & index_manager() noexcept;

    [[nodiscard]]
    const index::IndexManager & index_manager() const noexcept;

    [[nodiscard]]
    std::mutex & mutex() noexcept;

    [[nodiscard]]
    executor::DdlMutationHandler * ddl_handler() noexcept;

private:
    meta::MetaEngine meta_;                                         ///< 元数据
    storage::StorageEngine storage_;                                        ///< 存储引擎
    index::IndexManager index_manager_;                                     ///< 索引管理器
    std::unique_ptr<persistence::PersistenceController> persistence_;       ///< 持久化控制器
    std::mutex mutex_;                                                      ///< 互斥锁
};

} // namespace litedb::core::engine

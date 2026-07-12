#pragma once

#include <expected>
#include <optional>
#include <string_view>

#include "core/meta/meta_engine.hpp"
#include "core/common/ids.hpp"
#include "core/engine/database_instance.hpp"
#include "core/engine/engine_error.hpp"
#include "core/engine/session.hpp"
#include "core/executor/execution_result.hpp"
#include "core/index/index_manager.hpp"
#include "core/storage/storage_manager.hpp"

namespace litedb::core::engine
{

/**
 * @brief 数据库 Engine
 */
class Engine
{
public:
    Engine();

public:
    /**
     * @brief 执行 SQL
     * @param sql SQL 字符串
     * @return 执行结果
     */
    [[nodiscard]]
    std::expected<executor::ExecutionResult, EngineError> execute_sql(std::string_view sql);

    /**
     * @brief 获取当前数据库 ID
     * @return 当前数据库 ID
     */
    [[nodiscard]]
    std::optional<common::DatabaseId> current_database_id() const noexcept;

    /**
     * @brief 获取目录
     * @return 目录
     */
    [[nodiscard]]
    meta::MetaEngine & meta() noexcept;

    /**
     * @brief 获取目录
     * @return 目录
     */
    [[nodiscard]]
    const meta::MetaEngine & meta() const noexcept;

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
     * @brief 获取索引管理器
     * @return 索引管理器
     */
    [[nodiscard]]
    index::IndexManager & index_manager() noexcept;

    /**
     * @brief 获取索引管理器
     * @return 索引管理器
     */
    [[nodiscard]]
    const index::IndexManager & index_manager() const noexcept;

private:
    DatabaseInstance instance_;              ///< 数据库实例
    Session session_;                        ///< 会话
};

} // namespace litedb::core::engine

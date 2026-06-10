#pragma once

#include <expected>
#include <optional>
#include <string_view>

#include "core/binder/session_context.hpp"
#include "core/catalog/in_memory_catalog.hpp"
#include "core/engine/engine_error.hpp"
#include "core/executor/execution_result.hpp"
#include "core/storage/storage_manager.hpp"

namespace litedb::core::engine
{

/**
 * @brief 数据库 Engine
 */
class Engine
{
public:
    Engine() = default;

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

private:
    catalog::InMemoryCatalog catalog_;      ///< 内存目录
    storage::StorageManager storage_;       ///< 存储管理器
    binder::SessionContext session_;        ///< 会话上下文
};

} // namespace litedb::core::engine

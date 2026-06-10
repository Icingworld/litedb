#pragma once

#include <expected>
#include <optional>
#include <string_view>

#include "core/binder/session_context.hpp"
#include "core/common/ids.hpp"
#include "core/engine/database_instance.hpp"
#include "core/engine/engine_error.hpp"
#include "core/executor/execution_result.hpp"

namespace litedb::core::engine
{

/**
 * @brief 会话
 */
class Session
{
public:
    explicit Session(DatabaseInstance & instance) noexcept;

public:
    /**
     * @brief 执行 SQL
     * @param sql SQL
     * @return 执行结果
     */
    [[nodiscard]]
    std::expected<executor::ExecutionResult, EngineError> execute_sql(std::string_view sql);

public:
    /**
     * @brief 获取当前数据库 ID
     * @return 当前数据库 ID
     */
    [[nodiscard]]
    std::optional<common::DatabaseId> current_database_id() const noexcept;

private:
    DatabaseInstance * instance_;           ///< 数据库实例
    binder::SessionContext session_;        ///< 会话上下文
};

} // namespace litedb::core::engine

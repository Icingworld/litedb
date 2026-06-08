#pragma once

#include <optional>

#include "core/common/ids.hpp"

namespace litedb::core::binder
{

/**
 * @brief 会话上下文
 * @details 用于临时管理当前数据库 ID，后续可能扩展为专用的管理器
 */
struct SessionContext
{
    std::optional<common::DatabaseId> current_database_id;    ///< 当前数据库 ID
};

} // namespace litedb::core::binder

#pragma once

#include <optional>

#include "core/common/ids.hpp"

namespace litedb::core::binder
{

// 会话上下文
// 后续可能扩展为专用的管理器
struct SessionContext
{
    std::optional<common::DatabaseId> current_database_id;
};

} // namespace litedb::core::binder

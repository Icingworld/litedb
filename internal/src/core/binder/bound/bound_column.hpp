#pragma once

#include <string>

#include "core/common/ids.hpp"
#include "core/common/logical_type.hpp"

namespace litedb::core::binder::bound
{

/**
 * @brief 绑定列
 */
struct BoundColumn
{
    common::ColumnId column_id {0};     // 列 ID
    std::string name;                   // 列名称
    common::LogicalType type;           // 列类型
    bool nullable {true};               // 是否可为 NULL
};

} // namespace litedb::core::binder::bound

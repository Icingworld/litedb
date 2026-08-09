#pragma once

#include <string>

#include "core/common/ids.hpp"
#include "core/common/logical_type.hpp"

namespace litedb::core::binder::bound
{

// 绑定列
struct BoundColumn
{
    common::ColumnId column_id {0};
    std::string name;
    common::LogicalType type;
    bool nullable {true};
};

} // namespace litedb::core::binder::bound

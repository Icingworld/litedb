#pragma once

#include <memory>

#include "core/binder/bound/expression/bound_expression.hpp"
#include "core/common/ids.hpp"

namespace litedb::core::binder::bound
{

// 绑定赋值
struct BoundAssignment
{
    common::ColumnId column_id;
    std::unique_ptr<BoundExpression> value;
};

} // namespace litedb::core::binder::bound

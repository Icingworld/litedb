#pragma once

#include <memory>

#include "core/binder/bound/expression/bound_expression.hpp"
#include "core/common/ids.hpp"

namespace litedb::core::binder::bound
{

/**
 * @brief 绑定赋值
 */
struct BoundAssignment
{
    common::ColumnId column_id;                     // 列
    std::unique_ptr<BoundExpression> value;         // 值
};

} // namespace litedb::core::binder::bound

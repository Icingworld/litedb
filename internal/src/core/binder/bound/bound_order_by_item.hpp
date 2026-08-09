#pragma once

#include <memory>

#include "core/binder/bound/expression/bound_expression.hpp"

namespace litedb::core::binder::bound
{

// 排序项
struct BoundOrderByItem
{
    std::unique_ptr<BoundExpression> expression;
    bool ascending {true};
};

} // namespace litedb::core::binder::bound

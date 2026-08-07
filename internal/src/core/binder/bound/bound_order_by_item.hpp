#pragma once

#include <memory>

#include "core/binder/bound/expression/bound_expression.hpp"

namespace litedb::core::binder::bound
{

/**
 * @brief 排序项
 */
struct BoundOrderByItem
{
    std::unique_ptr<BoundExpression> expression;    // 排序表达式
    bool ascending {true};                          // 是否升序
};

} // namespace litedb::core::binder::bound

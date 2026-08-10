#pragma once

#include <memory>

namespace litedb::core::binder::bound
{

class BoundExpression;

}

namespace litedb::core::optimizer
{

struct OptimizerOptions;

}

namespace litedb::core::optimizer::detail
{

// 判断布尔常量表达式
[[nodiscard]]
bool is_boolean_literal(
    const binder::bound::BoundExpression & expression,
    bool value
) noexcept;

// 重写表达式
[[nodiscard]]
std::unique_ptr<binder::bound::BoundExpression> rewrite_expression(
    std::unique_ptr<binder::bound::BoundExpression> expression,
    const OptimizerOptions & options
);

} // namespace litedb::core::optimizer::detail

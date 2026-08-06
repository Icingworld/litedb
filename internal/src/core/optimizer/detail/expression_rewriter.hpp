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

/**
 * @brief 判断布尔常量表达式
 * @param expression 表达式
 * @param value 值
 * @return 是否为布尔常量表达式
 */
[[nodiscard]]
bool is_boolean_literal(
    const binder::bound::BoundExpression & expression,
    bool value
) noexcept;

/**
 * @brief 重写表达式
 * @param expression 表达式
 * @param options 优化器选项
 * @return 重写后的表达式
 */
[[nodiscard]]
std::unique_ptr<binder::bound::BoundExpression> rewrite_expression(
    std::unique_ptr<binder::bound::BoundExpression> expression,
    const OptimizerOptions & options
);

} // namespace litedb::core::optimizer::detail

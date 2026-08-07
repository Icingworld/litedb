#pragma once

#include <memory>

#include "core/binder/bound/expression/bound_expression.hpp"

namespace litedb::core::binder::bound
{

/**
 * @brief 绑定 LIKE 表达式
 */
class BoundLikeExpression final : public BoundExpression
{
public:
    BoundLikeExpression(
        std::unique_ptr<BoundExpression> expression,
        std::unique_ptr<BoundExpression> pattern
    );

public:
    /**
     * @brief 获取表达式
     * @return 表达式
     */
    [[nodiscard]]
    const BoundExpression & expression() const noexcept;

    /**
     * @brief 移出 LIKE 目标表达式
     * @return 目标表达式所有权
     */
    [[nodiscard]]
    std::unique_ptr<BoundExpression> take_expression() noexcept;

    /**
     * @brief 获取模式
     * @return 模式
     */
    [[nodiscard]]
    const BoundExpression & pattern() const noexcept;

    /**
     * @brief 移出 LIKE 模式
     * @return 模式表达式所有权
     */
    [[nodiscard]]
    std::unique_ptr<BoundExpression> take_pattern() noexcept;

private:
    std::unique_ptr<BoundExpression> expression_;   // 表达式
    std::unique_ptr<BoundExpression> pattern_;      // 模式
};

} // namespace litedb::core::binder::bound

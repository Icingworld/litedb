#pragma once

#include <memory>

#include "core/binder/bound/expression/bound_expression.hpp"

namespace litedb::core::binder::bound
{

// 绑定 LIKE 表达式
class BoundLikeExpression final : public BoundExpression
{
public:
    BoundLikeExpression(
        std::unique_ptr<BoundExpression> expression,
        std::unique_ptr<BoundExpression> pattern
    );

public:
    // 获取表达式
    [[nodiscard]]
    const BoundExpression & expression() const noexcept;

    // 获取目标表达式所有权
    [[nodiscard]]
    std::unique_ptr<BoundExpression> take_expression() noexcept;

    // 获取模式
    [[nodiscard]]
    const BoundExpression & pattern() const noexcept;

    // 获取模式表达式所有权
    [[nodiscard]]
    std::unique_ptr<BoundExpression> take_pattern() noexcept;

private:
    std::unique_ptr<BoundExpression> expression_;
    std::unique_ptr<BoundExpression> pattern_;
};

} // namespace litedb::core::binder::bound

#pragma once

#include <memory>

#include "core/binder/bound/expression/bound_expression.hpp"

namespace litedb::core::binder::bound
{

// 绑定 BETWEEN 表达式
class BoundBetweenExpression final : public BoundExpression
{
public:
    BoundBetweenExpression(
        std::unique_ptr<BoundExpression> expression,
        std::unique_ptr<BoundExpression> lower,
        std::unique_ptr<BoundExpression> upper
    );

public:
    // 获取表达式
    [[nodiscard]]
    const BoundExpression & expression() const noexcept;

    // 获取目标表达式所有权
    [[nodiscard]]
    std::unique_ptr<BoundExpression> take_expression() noexcept;

    // 获取下界
    [[nodiscard]]
    const BoundExpression & lower() const noexcept;

    // 获取下界所有权
    [[nodiscard]]
    std::unique_ptr<BoundExpression> take_lower() noexcept;

    // 获取上界
    [[nodiscard]]
    const BoundExpression & upper() const noexcept;

    // 获取上界所有权
    [[nodiscard]]
    std::unique_ptr<BoundExpression> take_upper() noexcept;

private:
    std::unique_ptr<BoundExpression> expression_;
    std::unique_ptr<BoundExpression> lower_;
    std::unique_ptr<BoundExpression> upper_;
};

} // namespace litedb::core::binder::bound

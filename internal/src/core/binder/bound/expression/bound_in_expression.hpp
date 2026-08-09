#pragma once

#include <memory>
#include <vector>

#include "core/binder/bound/expression/bound_expression.hpp"

namespace litedb::core::binder::bound
{

// 绑定 IN 表达式
class BoundInExpression final : public BoundExpression
{
public:
    BoundInExpression(
        std::unique_ptr<BoundExpression> expression,
        std::vector<std::unique_ptr<BoundExpression>> values
    );

public:
    // 获取表达式
    [[nodiscard]]
    const BoundExpression & expression() const noexcept;

    // 获取目标表达式所有权
    [[nodiscard]]
    std::unique_ptr<BoundExpression> take_expression() noexcept;

    // 获取值列表
    [[nodiscard]]
    const std::vector<std::unique_ptr<BoundExpression>> & values() const noexcept;

    // 获取值列表所有权
    [[nodiscard]]
    std::vector<std::unique_ptr<BoundExpression>> take_values() noexcept;

private:
    std::unique_ptr<BoundExpression> expression_;
    std::vector<std::unique_ptr<BoundExpression>> values_;
};

} // namespace litedb::core::binder::bound

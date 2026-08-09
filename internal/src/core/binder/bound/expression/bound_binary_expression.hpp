#pragma once

#include <memory>

#include "core/binder/bound/expression/bound_expression.hpp"
#include "core/common/types.hpp"

namespace litedb::core::binder::bound
{

// 绑定二元表达式
class BoundBinaryExpression final : public BoundExpression
{
public:
    BoundBinaryExpression(
        std::unique_ptr<BoundExpression> left,
        common::BinaryOperator op,
        std::unique_ptr<BoundExpression> right,
        common::LogicalType type
    );

public:
    // 获取左操作数
    [[nodiscard]]
    const BoundExpression & left() const noexcept;

    // 获取左操作数所有权
    [[nodiscard]]
    std::unique_ptr<BoundExpression> take_left() noexcept;

    // 获取二元操作符
    [[nodiscard]]
    common::BinaryOperator op() const noexcept;

    // 获取右操作数
    [[nodiscard]]
    const BoundExpression & right() const noexcept;

    // 获取右操作数所有权
    [[nodiscard]]
    std::unique_ptr<BoundExpression> take_right() noexcept;

private:
    std::unique_ptr<BoundExpression> left_;
    common::BinaryOperator op_;
    std::unique_ptr<BoundExpression> right_;
};

} // namespace litedb::core::binder::bound

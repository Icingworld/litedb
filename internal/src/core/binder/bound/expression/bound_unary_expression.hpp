#pragma once

#include <memory>

#include "core/binder/bound/expression/bound_expression.hpp"
#include "core/common/types.hpp"

namespace litedb::core::binder::bound
{

// 绑定一元表达式
class BoundUnaryExpression final : public BoundExpression
{
public:
    BoundUnaryExpression(
        common::UnaryOperator op,
        std::unique_ptr<BoundExpression> operand,
        common::LogicalType type
    );

public:
    // 获取一元操作符
    [[nodiscard]]
    common::UnaryOperator op() const noexcept;

    // 获取操作数
    [[nodiscard]]
    const BoundExpression & operand() const noexcept;

    // 获取操作数所有权
    [[nodiscard]]
    std::unique_ptr<BoundExpression> take_operand() noexcept;

private:
    common::UnaryOperator op_;
    std::unique_ptr<BoundExpression> operand_;
};

} // namespace litedb::core::binder::bound

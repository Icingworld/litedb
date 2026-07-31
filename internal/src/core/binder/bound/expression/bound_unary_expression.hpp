#pragma once

#include <memory>

#include "core/binder/bound/expression/bound_expression.hpp"
#include "core/common/types.hpp"

namespace litedb::core::binder::bound
{

/**
 * @brief 绑定一元表达式
 */
class BoundUnaryExpression final : public BoundExpression
{
public:
    BoundUnaryExpression(
        common::UnaryOperator op,
        std::unique_ptr<BoundExpression> operand,
        common::LogicalType type
    );

public:
    /**
     * @brief 获取一元操作符
     * @return 一元操作符
     */
    [[nodiscard]]
    common::UnaryOperator op() const noexcept;

    /**
     * @brief 获取操作数
     * @return 操作数
     */
    [[nodiscard]]
    const BoundExpression & operand() const noexcept;

private:
    common::UnaryOperator op_;                      ///< 一元操作符
    std::unique_ptr<BoundExpression> operand_;      ///< 操作数
};

} // namespace litedb::core::binder::bound

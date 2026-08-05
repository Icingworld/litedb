#pragma once

#include <memory>

#include "core/binder/bound/expression/bound_expression.hpp"

namespace litedb::core::binder::bound
{

/**
 * @brief 绑定 CAST 表达式
 */
class BoundCastExpression final : public BoundExpression
{
public:
    BoundCastExpression(
        std::unique_ptr<BoundExpression> expression,
        common::LogicalType target_type
    );

public:
    /**
     * @brief 获取表达式
     * @return 表达式
     */
    [[nodiscard]]
    const BoundExpression & expression() const noexcept;

    /**
     * @brief 移出 CAST 内部表达式
     * @return 内部表达式所有权
     */
    [[nodiscard]]
    std::unique_ptr<BoundExpression> take_expression() noexcept;

private:
    std::unique_ptr<BoundExpression> expression_;    ///< 表达式
};

} // namespace litedb::core::binder::bound

#pragma once

#include <memory>
#include <vector>

#include "core/binder/bound/expression/bound_expression.hpp"

namespace litedb::core::binder::bound
{

/**
 * @brief 绑定 IN 表达式
 */
class BoundInExpression final : public BoundExpression
{
public:
    BoundInExpression(
        std::unique_ptr<BoundExpression> expression,
        std::vector<std::unique_ptr<BoundExpression>> values
    );

public:
    /**
     * @brief 获取表达式
     * @return 表达式
     */
    [[nodiscard]]
    const BoundExpression & expression() const noexcept;

    /**
     * @brief 移出 IN 目标表达式
     * @return 目标表达式所有权
     */
    [[nodiscard]]
    std::unique_ptr<BoundExpression> take_expression() noexcept;

    /**
     * @brief 获取值列表
     * @return 值列表
     */
    [[nodiscard]]
    const std::vector<std::unique_ptr<BoundExpression>> & values() const noexcept;

    /**
     * @brief 移出 IN 值列表
     * @return 值列表所有权
     */
    [[nodiscard]]
    std::vector<std::unique_ptr<BoundExpression>> take_values() noexcept;

private:
    std::unique_ptr<BoundExpression> expression_;               ///< 表达式
    std::vector<std::unique_ptr<BoundExpression>> values_;      ///< 值列表
};

} // namespace litedb::core::binder::bound

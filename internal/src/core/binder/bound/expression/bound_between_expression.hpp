#pragma once

#include <memory>

#include "core/binder/bound/expression/bound_expression.hpp"

namespace litedb::core::binder::bound
{

/**
 * @brief 绑定 BETWEEN 表达式
 */
class BoundBetweenExpression final : public BoundExpression
{
public:
    BoundBetweenExpression(
        std::unique_ptr<BoundExpression> expression,
        std::unique_ptr<BoundExpression> lower,
        std::unique_ptr<BoundExpression> upper
    );

public:
    /**
     * @brief 获取表达式
     * @return 表达式
     */
    [[nodiscard]]
    const BoundExpression & expression() const noexcept;

    /**
     * @brief 移出 BETWEEN 目标表达式
     * @return 目标表达式所有权
     */
    [[nodiscard]]
    std::unique_ptr<BoundExpression> take_expression() noexcept;

    /**
     * @brief 获取下界
     * @return 下界
     */
    [[nodiscard]]
    const BoundExpression & lower() const noexcept;

    /**
     * @brief 移出 BETWEEN 下界
     * @return 下界所有权
     */
    [[nodiscard]]
    std::unique_ptr<BoundExpression> take_lower() noexcept;

    /**
     * @brief 获取上界
     * @return 上界
     */
    [[nodiscard]]
    const BoundExpression & upper() const noexcept;

    /**
     * @brief 移出 BETWEEN 上界
     * @return 上界所有权
     */
    [[nodiscard]]
    std::unique_ptr<BoundExpression> take_upper() noexcept;

private:
    std::unique_ptr<BoundExpression> expression_;   ///< 表达式
    std::unique_ptr<BoundExpression> lower_;        ///< 下界
    std::unique_ptr<BoundExpression> upper_;        ///< 上界
};

} // namespace litedb::core::binder::bound

#pragma once

#include <memory>

#include "core/binder/bound/expression/bound_expression.hpp"

namespace litedb::core::binder::bound
{

/**
 * @brief BETWEEN 表达式节点
 * @details 示例：expression BETWEEN lower AND upper
 */
class BoundBetweenExpression final : public BoundExpression
{
public:
    BoundBetweenExpression(
        std::unique_ptr<BoundExpression> expression,
        std::unique_ptr<BoundExpression> lower,
        std::unique_ptr<BoundExpression> upper,
        parser::ast::AstNodeLocation location
    );

public:
    /**
     * @brief 获取表达式
     * @return 表达式
     */
    [[nodiscard]]
    const BoundExpression & expression() const noexcept;

    /**
     * @brief 获取下界
     * @return 下界
     */
    [[nodiscard]]
    const BoundExpression & lower() const noexcept;

    /**
     * @brief 获取上界
     * @return 上界
     */
    [[nodiscard]]
    const BoundExpression & upper() const noexcept;

    /**
     * @brief 接受访问器访问
     * @param visitor 访问器
     */
    void accept(BoundExpressionVisitor & visitor) const override;

private:
    std::unique_ptr<BoundExpression> expression_;   ///< 表达式
    std::unique_ptr<BoundExpression> lower_;        ///< 下界
    std::unique_ptr<BoundExpression> upper_;        ///< 上界
};

} // namespace litedb::core::binder::bound

#pragma once

#include <memory>

#include "core/binder/bound/expression/bound_expression.hpp"

namespace litedb::core::binder::bound
{

/**
 * @brief CAST 表达式节点
 * @details 示例：expression AS target_type
 */
class BoundCastExpression final : public BoundExpression
{
public:
    BoundCastExpression(
        std::unique_ptr<BoundExpression> expression,
        common::LogicalType target_type,
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
     * @brief 接受访问器访问
     * @param visitor 访问器
     */
    void accept(BoundExpressionVisitor & visitor) const override;

private:
    std::unique_ptr<BoundExpression> expression_;    ///< 表达式
};

} // namespace litedb::core::binder::bound

#pragma once

#include <memory>

#include "core/binder/bound/expression/bound_expression.hpp"

namespace litedb::core::binder::bound
{

/**
 * @brief LIKE 表达式节点
 * @details 示例：expression LIKE pattern
 */
class BoundLikeExpression final : public BoundExpression
{
public:
    BoundLikeExpression(
        std::unique_ptr<BoundExpression> expression,
        std::unique_ptr<BoundExpression> pattern,
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
     * @brief 获取模式
     * @return 模式
     */
    [[nodiscard]]
    const BoundExpression & pattern() const noexcept;

private:
    std::unique_ptr<BoundExpression> expression_;   ///< 表达式
    std::unique_ptr<BoundExpression> pattern_;      ///< 模式
};

} // namespace litedb::core::binder::bound

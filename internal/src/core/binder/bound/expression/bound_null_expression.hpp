#pragma once

#include <memory>

#include "core/binder/bound/expression/bound_expression.hpp"

namespace litedb::core::binder::bound
{

/**
 * @brief NULL 表达式节点
 * @details 示例：NULL
 */
class BoundNullExpression final : public BoundExpression
{
public:
    BoundNullExpression(common::LogicalType type, parser::ast::AstNodeLocation location);

    /**
     * @brief 接受访问器访问
     * @param visitor 访问器
     */
    void accept(BoundExpressionVisitor & visitor) const override;

    /**
     * @brief 深拷贝表达式
     * @return 表达式副本
     */
    [[nodiscard]]
    std::unique_ptr<BoundExpression> clone() const override;
};

} // namespace litedb::core::binder::bound

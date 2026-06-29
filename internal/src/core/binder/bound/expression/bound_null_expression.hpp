#pragma once

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
};

} // namespace litedb::core::binder::bound

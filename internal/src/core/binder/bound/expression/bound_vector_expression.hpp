#pragma once

#include <memory>
#include <vector>

#include "core/binder/bound/expression/bound_expression.hpp"

namespace litedb::core::binder::bound
{

/**
 * @brief 向量表达式节点
 * @details 示例：[value1, value2, ...]
 */
class BoundVectorExpression final : public BoundExpression
{
public:
    BoundVectorExpression(
        std::vector<std::unique_ptr<BoundExpression>> elements,
        common::LogicalType type,
        parser::ast::AstNodeLocation location
    );

public:
    /**
     * @brief 获取元素列表
     * @return 元素列表
     */
    [[nodiscard]]
    const std::vector<std::unique_ptr<BoundExpression>> & elements() const noexcept;

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

private:
    std::vector<std::unique_ptr<BoundExpression>> elements_;      ///< 元素列表
};

} // namespace litedb::core::binder::bound

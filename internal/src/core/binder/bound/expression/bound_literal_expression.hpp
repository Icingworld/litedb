#pragma once

#include <memory>
#include <string>

#include "core/binder/bound/expression/bound_expression.hpp"

namespace litedb::core::binder::bound
{

/**
 * @brief 常量表达式节点
 * @details 示例：value
 */
class BoundLiteralExpression final : public BoundExpression
{
public:
    BoundLiteralExpression(common::LogicalType type, std::string value, parser::ast::AstNodeLocation location);

public:
    /**
     * @brief 获取常量值
     * @return 常量值
     */
    [[nodiscard]]
    const std::string & value() const noexcept;

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
    std::string value_;     ///< 常量值
};

} // namespace litedb::core::binder::bound

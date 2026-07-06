#pragma once

#include <memory>
#include <optional>
#include <string>

#include "core/binder/bound/expression/bound_expression.hpp"

namespace litedb::core::binder::bound
{

/**
 * @brief 通配符表达式节点
 * @details 示例：*
 */
class BoundWildcardExpression final : public BoundExpression
{
public:
    BoundWildcardExpression(std::optional<std::string> qualifier, parser::ast::AstNodeLocation location);

public:
    /**
     * @brief 获取限定符
     * @return 限定符
     */
    [[nodiscard]]
    const std::optional<std::string> & qualifier() const noexcept;

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
    std::optional<std::string> qualifier_;    ///< 限定符
};

} // namespace litedb::core::binder::bound

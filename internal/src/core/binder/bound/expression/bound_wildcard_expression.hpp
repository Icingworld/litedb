#pragma once

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

private:
    std::optional<std::string> qualifier_;    ///< 限定符
};

} // namespace litedb::core::binder::bound

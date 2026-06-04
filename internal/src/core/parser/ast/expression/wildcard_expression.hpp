#pragma once

#include <optional>
#include <string>

#include "core/parser/ast/expression/expression_node.hpp"

namespace litedb::core::parser::ast
{

class WildcardExpression final : public ExpressionNode
{
public:
    explicit WildcardExpression(AstNodeLocation location) noexcept;

    WildcardExpression(std::optional<std::string> qualifier, AstNodeLocation location) noexcept;

    [[nodiscard]]
    AstNodeKind kind() const noexcept override;

    [[nodiscard]]
    const std::optional<std::string> & qualifier() const noexcept;

private:
    std::optional<std::string> qualifier_;
};

} // namespace litedb::core::parser::ast

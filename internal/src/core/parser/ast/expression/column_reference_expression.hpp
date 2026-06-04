#pragma once

#include <optional>
#include <string>

#include "core/parser/ast/expression/expression_node.hpp"

namespace litedb::core::parser::ast
{

class ColumnReferenceExpression final : public ExpressionNode
{
public:
    ColumnReferenceExpression(std::optional<std::string> qualifier, std::string column, AstNodeLocation location) noexcept;

    [[nodiscard]]
    AstNodeKind kind() const noexcept override;

    [[nodiscard]]
    const std::optional<std::string> & qualifier() const noexcept;

    [[nodiscard]]
    const std::string & column() const noexcept;

private:
    std::optional<std::string> qualifier_;
    std::string column_;
};

} // namespace litedb::core::parser::ast

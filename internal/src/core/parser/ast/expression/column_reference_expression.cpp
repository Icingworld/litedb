#include "core/parser/ast/expression/column_reference_expression.hpp"

#include <utility>

namespace litedb::core::parser::ast
{

ColumnReferenceExpression::ColumnReferenceExpression(
    std::optional<std::string> qualifier,
    std::string column,
    AstNodeLocation location
) noexcept
    : ExpressionNode(location)
    , qualifier_(std::move(qualifier))
    , column_(std::move(column))
{
}

AstNodeKind ColumnReferenceExpression::kind() const noexcept
{
    return AstNodeKind::ColumnReference;
}

const std::optional<std::string> & ColumnReferenceExpression::qualifier() const noexcept
{
    return qualifier_;
}

const std::string & ColumnReferenceExpression::column() const noexcept
{
    return column_;
}

} // namespace litedb::core::parser::ast

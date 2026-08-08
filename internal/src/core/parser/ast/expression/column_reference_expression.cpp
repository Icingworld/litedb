#include "core/parser/ast/expression/column_reference_expression.hpp"

#include <cassert>
#include <utility>

namespace litedb::core::parser::ast
{

ColumnReferenceExpression::ColumnReferenceExpression(
    std::optional<std::string> qualifier,
    std::string column_name,
    AstNodeLocation location
)
    : ExpressionNode(location)
    , qualifier_(std::move(qualifier))
    , column_name_(std::move(column_name))
{
    if (qualifier_.has_value()) {
        assert(!qualifier_.value().empty());
    }
    assert(!column_name_.empty());
}

AstNodeKind ColumnReferenceExpression::kind() const noexcept
{
    return AstNodeKind::ColumnReference;
}

const std::optional<std::string> & ColumnReferenceExpression::qualifier() const noexcept
{
    return qualifier_;
}

const std::string & ColumnReferenceExpression::column_name() const noexcept
{
    return column_name_;
}

} // namespace litedb::core::parser::ast

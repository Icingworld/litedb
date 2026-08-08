#include "core/parser/ast/expression/in_expression.hpp"

#include <cassert>
#include <utility>

namespace litedb::core::parser::ast
{

InExpression::InExpression(
    std::unique_ptr<ExpressionNode> expression,
    std::vector<std::unique_ptr<ExpressionNode>> values,
    AstNodeLocation location
)
    : ExpressionNode(location)
    , expression_(std::move(expression))
    , values_(std::move(values))
{
    assert(expression_ != nullptr);
    assert(!values_.empty());
    for (const auto & value : values_) {
        assert(value != nullptr);
    }
}

AstNodeKind InExpression::kind() const noexcept
{
    return AstNodeKind::In;
}

const ExpressionNode & InExpression::expression() const noexcept
{
    return *expression_;
}

const std::vector<std::unique_ptr<ExpressionNode>> & InExpression::values() const noexcept
{
    return values_;
}

} // namespace litedb::core::parser::ast

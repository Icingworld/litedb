#include "core/parser/ast/expression/expression_node.hpp"

namespace litedb::core::parser::ast
{

ExpressionNode::ExpressionNode(AstNodeLocation location) noexcept
    : AstNode(location)
{
}

} // namespace litedb::core::parser::ast

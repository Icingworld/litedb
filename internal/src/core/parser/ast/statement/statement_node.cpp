#include "core/parser/ast/statement/statement_node.hpp"

namespace litedb::core::parser::ast
{

StatementNode::StatementNode(AstNodeLocation location) noexcept
    : AstNode(location)
{}

} // namespace litedb::core::parser::ast

#include "core/parser/ast/ast_node.hpp"

namespace litedb::core::parser::ast
{

AstNode::AstNode(AstNodeLocation location) noexcept
    : location_(location)
{
}

AstNodeLocation AstNode::location() const noexcept
{
    return location_;
}

} // namespace litedb::core::parser::ast
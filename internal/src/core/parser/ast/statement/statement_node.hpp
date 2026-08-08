#pragma once

#include "core/parser/ast/ast_node.hpp"

namespace litedb::core::parser::ast
{

// 语句节点基类
class StatementNode : public AstNode
{
protected:
    explicit StatementNode(AstNodeLocation location) noexcept;
};

} // namespace litedb::core::parser::ast

#pragma once

#include "core/parser/ast/ast_node.hpp"

namespace litedb::core::parser::ast
{

/**
 * @brief 表达式节点基类
 */
class ExpressionNode : public AstNode
{
protected:
    explicit ExpressionNode(AstNodeLocation location) noexcept;
};

} // namespace litedb::core::parser::ast

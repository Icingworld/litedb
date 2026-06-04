#pragma once

#include "core/parser/ast/schema.hpp"
#include "core/parser/ast/statement/statement_node.hpp"

namespace litedb::core::parser::ast
{

class ShowStatement final : public StatementNode
{
public:
    ShowStatement(SchemaObjectType object_type, AstNodeLocation location) noexcept;

    [[nodiscard]]
    AstNodeKind kind() const noexcept override;

    [[nodiscard]]
    SchemaObjectType object_type() const noexcept;

private:
    SchemaObjectType object_type_;
};

} // namespace litedb::core::parser::ast

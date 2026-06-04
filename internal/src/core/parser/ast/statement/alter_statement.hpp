#pragma once

#include <string>

#include "core/parser/ast/schema.hpp"
#include "core/parser/ast/statement/statement_node.hpp"

namespace litedb::core::parser::ast
{

class AlterStatement final : public StatementNode
{
public:
    AlterStatement(SchemaObjectType object_type, std::string name, AstNodeLocation location) noexcept;

    [[nodiscard]]
    AstNodeKind kind() const noexcept override;

    [[nodiscard]]
    SchemaObjectType object_type() const noexcept;

    [[nodiscard]]
    const std::string & name() const noexcept;

private:
    SchemaObjectType object_type_;
    std::string name_;
};

} // namespace litedb::core::parser::ast

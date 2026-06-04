#include "core/parser/ast/statement/alter_statement.hpp"

#include <utility>

namespace litedb::core::parser::ast
{

AlterStatement::AlterStatement(
    SchemaObjectType object_type,
    std::string name,
    AstNodeLocation location
) noexcept
    : StatementNode(location)
    , object_type_(object_type)
    , name_(std::move(name))
{
}

AstNodeKind AlterStatement::kind() const noexcept
{
    return AstNodeKind::Alter;
}

SchemaObjectType AlterStatement::object_type() const noexcept
{
    return object_type_;
}

const std::string & AlterStatement::name() const noexcept
{
    return name_;
}

} // namespace litedb::core::parser::ast

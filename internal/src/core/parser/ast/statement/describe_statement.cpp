#include "core/parser/ast/statement/describe_statement.hpp"

#include <utility>

namespace litedb::core::parser::ast
{

DescribeStatement::DescribeStatement(
    SchemaObjectType object_type,
    std::string name,
    AstNodeLocation location
) noexcept
    : StatementNode(location)
    , object_type_(object_type)
    , name_(std::move(name))
{
}

AstNodeKind DescribeStatement::kind() const noexcept
{
    return AstNodeKind::Describe;
}

SchemaObjectType DescribeStatement::object_type() const noexcept
{
    return object_type_;
}

const std::string & DescribeStatement::name() const noexcept
{
    return name_;
}

} // namespace litedb::core::parser::ast

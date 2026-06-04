#include "core/parser/ast/statement/drop_statement.hpp"

#include <utility>

namespace litedb::core::parser::ast
{

DropStatement::DropStatement(TokenType object_type, std::string name, bool if_exists, AstNodeLocation location) noexcept
    : StatementNode(location)
    , object_type_(object_type)
    , name_(std::move(name))
    , if_exists_(if_exists)
{
}

AstNodeKind DropStatement::kind() const noexcept
{
    return AstNodeKind::Drop;
}

TokenType DropStatement::object_type() const noexcept
{
    return object_type_;
}

const std::string & DropStatement::name() const noexcept
{
    return name_;
}

bool DropStatement::if_exists() const noexcept
{
    return if_exists_;
}

} // namespace litedb::core::parser::ast

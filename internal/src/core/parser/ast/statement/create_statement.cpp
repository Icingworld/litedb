#include "core/parser/ast/statement/create_statement.hpp"

#include <utility>

namespace litedb::core::parser::ast
{

CreateStatement::CreateStatement(
    TokenType object_type,
    std::string name,
    bool if_not_exists,
    AstNodeLocation location
) noexcept
    : StatementNode(location)
    , object_type_(object_type)
    , name_(std::move(name))
    , if_not_exists_(if_not_exists)
{
}

AstNodeKind CreateStatement::kind() const noexcept
{
    return AstNodeKind::Create;
}

TokenType CreateStatement::object_type() const noexcept
{
    return object_type_;
}

const std::string & CreateStatement::name() const noexcept
{
    return name_;
}

bool CreateStatement::if_not_exists() const noexcept
{
    return if_not_exists_;
}

} // namespace litedb::core::parser::ast

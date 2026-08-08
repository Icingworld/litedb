#include "core/parser/ast/statement/use_statement.hpp"

#include <cassert>
#include <utility>

namespace litedb::core::parser::ast
{

UseStatement::UseStatement(std::string database_name, AstNodeLocation location)
    : StatementNode(location)
    , database_name_(std::move(database_name))
{
    assert(!database_name_.empty());
}

AstNodeKind UseStatement::kind() const noexcept
{
    return AstNodeKind::Use;
}

const std::string & UseStatement::database_name() const noexcept
{
    return database_name_;
}

} // namespace litedb::core::parser::ast

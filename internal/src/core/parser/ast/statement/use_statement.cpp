#include "core/parser/ast/statement/use_statement.hpp"

#include <utility>

namespace litedb::core::parser::ast
{

UseStatement::UseStatement(std::string database, AstNodeLocation location) noexcept
    : StatementNode(location)
    , database_(std::move(database))
{
}

AstNodeKind UseStatement::kind() const noexcept
{
    return AstNodeKind::Use;
}

const std::string & UseStatement::database() const noexcept
{
    return database_;
}

} // namespace litedb::core::parser::ast

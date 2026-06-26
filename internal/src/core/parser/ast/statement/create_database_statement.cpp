#include "core/parser/ast/statement/create_database_statement.hpp"

#include <utility>

namespace litedb::core::parser::ast
{

CreateDatabaseStatement::CreateDatabaseStatement(
    std::string database,
    bool if_not_exists,
    AstNodeLocation location
) noexcept
    : StatementNode(location)
    , database_(std::move(database))
    , if_not_exists_(if_not_exists)
{
}

AstNodeKind CreateDatabaseStatement::kind() const noexcept
{
    return AstNodeKind::CreateDatabase;
}

void CreateDatabaseStatement::accept(AstNodeVisitor & visitor) const
{
    visitor.visit(*this);
}

const std::string & CreateDatabaseStatement::database() const noexcept
{
    return database_;
}

bool CreateDatabaseStatement::if_not_exists() const noexcept
{
    return if_not_exists_;
}

} // namespace litedb::core::parser::ast

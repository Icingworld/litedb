#include "core/parser/ast/statement/create_database_statement.hpp"

#include <cassert>
#include <utility>

namespace litedb::core::parser::ast
{

CreateDatabaseStatement::CreateDatabaseStatement(
    std::string database_name,
    bool if_not_exists,
    AstNodeLocation location
)
    : StatementNode(location)
    , database_name_(std::move(database_name))
    , if_not_exists_(if_not_exists)
{
    assert(!database_name_.empty());
}

AstNodeKind CreateDatabaseStatement::kind() const noexcept
{
    return AstNodeKind::CreateDatabase;
}

const std::string & CreateDatabaseStatement::database_name() const noexcept
{
    return database_name_;
}

bool CreateDatabaseStatement::if_not_exists() const noexcept
{
    return if_not_exists_;
}

} // namespace litedb::core::parser::ast

#include "core/parser/ast/statement/drop_database_statement.hpp"

#include <utility>
#include <cassert>

namespace litedb::core::parser::ast
{

DropDatabaseStatement::DropDatabaseStatement(
    std::string database_name,
    bool if_exists,
    AstNodeLocation location
)
    : StatementNode(location)
    , database_name_(std::move(database_name))
    , if_exists_(if_exists)
{
    assert(!database_name_.empty());
}

AstNodeKind DropDatabaseStatement::kind() const noexcept
{
    return AstNodeKind::DropDatabase;
}

const std::string & DropDatabaseStatement::database_name() const noexcept
{
    return database_name_;
}

bool DropDatabaseStatement::if_exists() const noexcept
{
    return if_exists_;
}

} // namespace litedb::core::parser::ast

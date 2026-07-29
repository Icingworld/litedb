#include "core/parser/ast/statement/show_collections_statement.hpp"

#include <utility>

namespace litedb::core::parser::ast
{

ShowCollectionsStatement::ShowCollectionsStatement(
    std::optional<std::string> database_name,
    AstNodeLocation location
) noexcept
    : StatementNode(location)
    , database_name_(std::move(database_name))
{
}

AstNodeKind ShowCollectionsStatement::kind() const noexcept
{
    return AstNodeKind::ShowCollections;
}

const std::optional<std::string> & ShowCollectionsStatement::database_name() const noexcept
{
    return database_name_;
}

} // namespace litedb::core::parser::ast

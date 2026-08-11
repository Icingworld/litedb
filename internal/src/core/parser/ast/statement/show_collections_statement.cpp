#include "core/parser/ast/statement/show_collections_statement.hpp"

#include <cassert>
#include <utility>

namespace litedb::core::parser::ast
{

ShowCollectionsStatement::ShowCollectionsStatement(
    std::optional<std::string> database_name,
    AstNodeLocation location
)
    : StatementNode(location)
    , database_name_(std::move(database_name))
{
    if (database_name_.has_value()) {
        assert(!database_name_.value().empty());
    }
}

AstNodeKind ShowCollectionsStatement::kind() const noexcept
{
    return AstNodeKind::ShowCollections;
}

std::optional<const std::string &> ShowCollectionsStatement::database_name() const noexcept
{
    return database_name_;
}

} // namespace litedb::core::parser::ast

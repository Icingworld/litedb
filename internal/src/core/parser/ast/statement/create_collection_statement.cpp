#include "core/parser/ast/statement/create_collection_statement.hpp"

#include <utility>

namespace litedb::core::parser::ast
{

CreateCollectionStatement::CreateCollectionStatement(
    std::string collection,
    bool if_not_exists,
    ColumnDefinitionList columns,
    AstNodeLocation location
) noexcept
    : StatementNode(location)
    , collection_(std::move(collection))
    , if_not_exists_(if_not_exists)
    , columns_(std::move(columns))
{
}

AstNodeKind CreateCollectionStatement::kind() const noexcept
{
    return AstNodeKind::CreateCollection;
}

void CreateCollectionStatement::accept(AstNodeVisitor & visitor) const
{
    visitor.visit(*this);
}

const std::string & CreateCollectionStatement::collection() const noexcept
{
    return collection_;
}

bool CreateCollectionStatement::if_not_exists() const noexcept
{
    return if_not_exists_;
}

const ColumnDefinitionList & CreateCollectionStatement::columns() const noexcept
{
    return columns_;
}

} // namespace litedb::core::parser::ast

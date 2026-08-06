#include "core/parser/ast/statement/create_collection_statement.hpp"

#include <utility>

namespace litedb::core::parser::ast
{

CreateCollectionStatement::CreateCollectionStatement(
    std::string collection_name,
    bool if_not_exists,
    ColumnDefinitionSyntaxList columns,
    std::optional<std::string> comment,
    AstNodeLocation location
) noexcept
    : StatementNode(location)
    , collection_name_(std::move(collection_name))
    , if_not_exists_(if_not_exists)
    , columns_(std::move(columns))
    , comment_(std::move(comment))
{
}

AstNodeKind CreateCollectionStatement::kind() const noexcept
{
    return AstNodeKind::CreateCollection;
}

const std::string & CreateCollectionStatement::collection_name() const noexcept
{
    return collection_name_;
}

bool CreateCollectionStatement::if_not_exists() const noexcept
{
    return if_not_exists_;
}

const ColumnDefinitionSyntaxList & CreateCollectionStatement::columns() const noexcept
{
    return columns_;
}

const std::optional<std::string> & CreateCollectionStatement::comment() const noexcept
{
    return comment_;
}

} // namespace litedb::core::parser::ast

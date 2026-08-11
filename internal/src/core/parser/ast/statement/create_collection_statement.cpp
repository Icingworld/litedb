#include "core/parser/ast/statement/create_collection_statement.hpp"

#include <cassert>
#include <utility>

namespace litedb::core::parser::ast
{

CreateCollectionStatement::CreateCollectionStatement(
    std::string collection_name,
    bool if_not_exists,
    std::vector<std::unique_ptr<ColumnDefinitionSyntax>> columns,
    std::optional<std::string> comment,
    AstNodeLocation location
)
    : StatementNode(location)
    , collection_name_(std::move(collection_name))
    , if_not_exists_(if_not_exists)
    , columns_(std::move(columns))
    , comment_(std::move(comment))
{
    assert(!collection_name_.empty());
    assert(!columns_.empty());
    for (const auto & column : columns_) {
        assert(column != nullptr);
        assert(!column->name.empty());
    }
    if (comment_.has_value()) {
        assert(!comment_.value().empty());
    }
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

const std::vector<std::unique_ptr<ColumnDefinitionSyntax>> &
CreateCollectionStatement::columns() const noexcept
{
    return columns_;
}

std::optional<const std::string &> CreateCollectionStatement::comment() const noexcept
{
    return comment_;
}

} // namespace litedb::core::parser::ast

#include "core/parser/ast/statement/drop_collection_statement.hpp"

#include <cassert>
#include <utility>

namespace litedb::core::parser::ast
{

DropCollectionStatement::DropCollectionStatement(
    std::string collection_name,
    bool if_exists,
    AstNodeLocation location
)
    : StatementNode(location)
    , collection_name_(std::move(collection_name))
    , if_exists_(if_exists)
{
    assert(!collection_name_.empty());
}

AstNodeKind DropCollectionStatement::kind() const noexcept
{
    return AstNodeKind::DropCollection;
}

const std::string & DropCollectionStatement::collection_name() const noexcept
{
    return collection_name_;
}

bool DropCollectionStatement::if_exists() const noexcept
{
    return if_exists_;
}

} // namespace litedb::core::parser::ast

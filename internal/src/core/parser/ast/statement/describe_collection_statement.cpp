#include "core/parser/ast/statement/describe_collection_statement.hpp"

#include <cassert>
#include <utility>

namespace litedb::core::parser::ast
{

DescribeCollectionStatement::DescribeCollectionStatement(
    std::string collection_name,
    AstNodeLocation location
)
    : StatementNode(location)
    , collection_name_(std::move(collection_name))
{
    assert(!collection_name_.empty());
}

AstNodeKind DescribeCollectionStatement::kind() const noexcept
{
    return AstNodeKind::DescribeCollection;
}

const std::string & DescribeCollectionStatement::collection_name() const noexcept
{
    return collection_name_;
}

} // namespace litedb::core::parser::ast

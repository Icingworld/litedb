#include "core/parser/ast/statement/describe_collection_statement.hpp"

#include <utility>

namespace litedb::core::parser::ast
{

DescribeCollectionStatement::DescribeCollectionStatement(
    std::string collection_name,
    AstNodeLocation location
) noexcept
    : StatementNode(location)
    , collection_name_(std::move(collection_name))
{
}

AstNodeKind DescribeCollectionStatement::kind() const noexcept
{
    return AstNodeKind::DescribeCollection;
}

void DescribeCollectionStatement::accept(AstNodeVisitor & visitor) const
{
    visitor.visit(*this);
}

const std::string & DescribeCollectionStatement::collection_name() const noexcept
{
    return collection_name_;
}

} // namespace litedb::core::parser::ast

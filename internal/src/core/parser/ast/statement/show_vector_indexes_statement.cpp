#include "core/parser/ast/statement/show_vector_indexes_statement.hpp"

#include <utility>

namespace litedb::core::parser::ast
{

ShowVectorIndexesStatement::ShowVectorIndexesStatement(std::string collection_name, AstNodeLocation location) noexcept
    : StatementNode(location)
    , collection_name_(std::move(collection_name))
{
}

AstNodeKind ShowVectorIndexesStatement::kind() const noexcept
{
    return AstNodeKind::ShowVectorIndexes;
}

void ShowVectorIndexesStatement::accept(AstNodeVisitor & visitor) const
{
    visitor.visit(*this);
}

const std::string & ShowVectorIndexesStatement::collection_name() const noexcept
{
    return collection_name_;
}

} // namespace litedb::core::parser::ast

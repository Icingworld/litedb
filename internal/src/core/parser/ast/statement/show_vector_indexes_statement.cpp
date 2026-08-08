#include "core/parser/ast/statement/show_vector_indexes_statement.hpp"

#include <utility>
#include <cassert>

namespace litedb::core::parser::ast
{

ShowVectorIndexesStatement::ShowVectorIndexesStatement(
    std::string collection_name,
    AstNodeLocation location
)
    : StatementNode(location)
    , collection_name_(std::move(collection_name))
{
    assert(!collection_name_.empty());
}

AstNodeKind ShowVectorIndexesStatement::kind() const noexcept
{
    return AstNodeKind::ShowVectorIndexes;
}

const std::string & ShowVectorIndexesStatement::collection_name() const noexcept
{
    return collection_name_;
}

} // namespace litedb::core::parser::ast

#include "core/parser/ast/statement/show_indexes_statement.hpp"

#include <cassert>
#include <utility>

namespace litedb::core::parser::ast
{

ShowIndexesStatement::ShowIndexesStatement(std::string collection_name, AstNodeLocation location)
    : StatementNode(location)
    , collection_name_(std::move(collection_name))
{
    assert(!collection_name_.empty());
}

AstNodeKind ShowIndexesStatement::kind() const noexcept
{
    return AstNodeKind::ShowIndexes;
}

const std::string & ShowIndexesStatement::collection_name() const noexcept
{
    return collection_name_;
}

} // namespace litedb::core::parser::ast

#include "core/parser/ast/statement/drop_vector_index_statement.hpp"

#include <utility>

namespace litedb::core::parser::ast
{

DropVectorIndexStatement::DropVectorIndexStatement(
    std::string index_name,
    std::string collection_name,
    bool if_exists,
    AstNodeLocation location
) noexcept
    : StatementNode(location)
    , index_name_(std::move(index_name))
    , collection_name_(std::move(collection_name))
    , if_exists_(if_exists)
{
}

AstNodeKind DropVectorIndexStatement::kind() const noexcept
{
    return AstNodeKind::DropVectorIndex;
}

void DropVectorIndexStatement::accept(AstNodeVisitor & visitor) const
{
    visitor.visit(*this);
}

const std::string & DropVectorIndexStatement::index_name() const noexcept
{
    return index_name_;
}

const std::string & DropVectorIndexStatement::collection_name() const noexcept
{
    return collection_name_;
}

bool DropVectorIndexStatement::if_exists() const noexcept
{
    return if_exists_;
}

} // namespace litedb::core::parser::ast

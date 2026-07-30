#include "core/parser/ast/statement/drop_vector_index_statement.hpp"

#include <utility>

namespace litedb::core::parser::ast
{

DropVectorIndexStatement::DropVectorIndexStatement(
    std::string vector_index_name,
    std::string collection_name,
    bool if_exists,
    AstNodeLocation location
) noexcept
    : StatementNode(location)
    , vector_index_name_(std::move(vector_index_name))
    , collection_name_(std::move(collection_name))
    , if_exists_(if_exists)
{
}

AstNodeKind DropVectorIndexStatement::kind() const noexcept
{
    return AstNodeKind::DropVectorIndex;
}

const std::string & DropVectorIndexStatement::vector_index_name() const noexcept
{
    return vector_index_name_;
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

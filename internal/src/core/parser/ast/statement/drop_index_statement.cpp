#include "core/parser/ast/statement/drop_index_statement.hpp"

#include <utility>

namespace litedb::core::parser::ast
{

DropIndexStatement::DropIndexStatement(
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

AstNodeKind DropIndexStatement::kind() const noexcept
{
    return AstNodeKind::DropIndex;
}

const std::string & DropIndexStatement::index_name() const noexcept
{
    return index_name_;
}

const std::string & DropIndexStatement::collection_name() const noexcept
{
    return collection_name_;
}

bool DropIndexStatement::if_exists() const noexcept
{
    return if_exists_;
}

} // namespace litedb::core::parser::ast

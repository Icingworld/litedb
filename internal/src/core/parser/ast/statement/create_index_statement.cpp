#include "core/parser/ast/statement/create_index_statement.hpp"

#include <utility>

namespace litedb::core::parser::ast
{

CreateIndexStatement::CreateIndexStatement(
    std::string index_name,
    std::string collection_name,
    std::string column_name,
    bool if_not_exists,
    CreateIndexMethod method,
    AstNodeLocation location
) noexcept
    : StatementNode(location)
    , index_name_(std::move(index_name))
    , collection_name_(std::move(collection_name))
    , column_name_(std::move(column_name))
    , if_not_exists_(if_not_exists)
    , method_(method)
{
}

AstNodeKind CreateIndexStatement::kind() const noexcept
{
    return AstNodeKind::CreateIndex;
}

const std::string & CreateIndexStatement::index_name() const noexcept
{
    return index_name_;
}

const std::string & CreateIndexStatement::collection_name() const noexcept
{
    return collection_name_;
}

const std::string & CreateIndexStatement::column_name() const noexcept
{
    return column_name_;
}

bool CreateIndexStatement::if_not_exists() const noexcept
{
    return if_not_exists_;
}

CreateIndexMethod CreateIndexStatement::method() const noexcept
{
    return method_;
}

} // namespace litedb::core::parser::ast

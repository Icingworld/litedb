#include "core/parser/ast/statement/create_vector_index_statement.hpp"

#include <utility>

namespace litedb::core::parser::ast
{

CreateVectorIndexStatement::CreateVectorIndexStatement(
    std::string index_name,
    std::string collection_name,
    std::string column_name,
    bool if_not_exists,
    CreateVectorIndexMethod method,
    VectorIndexOptions options,
    AstNodeLocation location
) noexcept
    : StatementNode(location)
    , index_name_(std::move(index_name))
    , collection_name_(std::move(collection_name))
    , column_name_(std::move(column_name))
    , if_not_exists_(if_not_exists)
    , method_(method)
    , options_(options)
{
}

AstNodeKind CreateVectorIndexStatement::kind() const noexcept
{
    return AstNodeKind::CreateVectorIndex;
}

const std::string & CreateVectorIndexStatement::index_name() const noexcept
{
    return index_name_;
}

const std::string & CreateVectorIndexStatement::collection_name() const noexcept
{
    return collection_name_;
}

const std::string & CreateVectorIndexStatement::column_name() const noexcept
{
    return column_name_;
}

bool CreateVectorIndexStatement::if_not_exists() const noexcept
{
    return if_not_exists_;
}

CreateVectorIndexMethod CreateVectorIndexStatement::method() const noexcept
{
    return method_;
}

const VectorIndexOptions & CreateVectorIndexStatement::options() const noexcept
{
    return options_;
}

} // namespace litedb::core::parser::ast

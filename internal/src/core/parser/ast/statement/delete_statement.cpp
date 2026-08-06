#include "core/parser/ast/statement/delete_statement.hpp"

#include <utility>

namespace litedb::core::parser::ast
{

DeleteStatement::DeleteStatement(
    std::string collection_name,
    std::unique_ptr<ExpressionNode> where,
    AstNodeLocation location
) noexcept
    : StatementNode(location)
    , collection_name_(std::move(collection_name))
    , where_(std::move(where))
{
}

AstNodeKind DeleteStatement::kind() const noexcept
{
    return AstNodeKind::Delete;
}

const std::string & DeleteStatement::collection_name() const noexcept
{
    return collection_name_;
}

const ExpressionNode * DeleteStatement::where() const noexcept
{
    return where_.get();
}

} // namespace litedb::core::parser::ast

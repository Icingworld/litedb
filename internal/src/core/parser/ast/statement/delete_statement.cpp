#include "core/parser/ast/statement/delete_statement.hpp"

#include <utility>

namespace litedb::core::parser::ast
{

DeleteStatement::DeleteStatement(
    std::string collection,
    std::unique_ptr<ExpressionNode> where,
    AstNodeLocation location
) noexcept
    : StatementNode(location)
    , collection_(std::move(collection))
    , where_(std::move(where))
{
}

AstNodeKind DeleteStatement::kind() const noexcept
{
    return AstNodeKind::Delete;
}

const std::string & DeleteStatement::collection() const noexcept
{
    return collection_;
}

const ExpressionNode * DeleteStatement::where() const noexcept
{
    return where_.get();
}

} // namespace litedb::core::parser::ast

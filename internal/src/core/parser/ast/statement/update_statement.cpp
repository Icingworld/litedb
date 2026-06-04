#include "core/parser/ast/statement/update_statement.hpp"

#include <utility>

namespace litedb::core::parser::ast
{

UpdateStatement::UpdateStatement(
    std::string collection,
    AssignmentList assignments,
    std::unique_ptr<ExpressionNode> where,
    AstNodeLocation location
) noexcept
    : StatementNode(location)
    , collection_(std::move(collection))
    , assignments_(std::move(assignments))
    , where_(std::move(where))
{
}

AstNodeKind UpdateStatement::kind() const noexcept
{
    return AstNodeKind::Update;
}

const std::string & UpdateStatement::collection() const noexcept
{
    return collection_;
}

const UpdateStatement::AssignmentList & UpdateStatement::assignments() const noexcept
{
    return assignments_;
}

const ExpressionNode * UpdateStatement::where() const noexcept
{
    return where_.get();
}

} // namespace litedb::core::parser::ast

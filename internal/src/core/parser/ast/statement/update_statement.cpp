#include "core/parser/ast/statement/update_statement.hpp"

#include <utility>

namespace litedb::core::parser::ast
{

UpdateStatement::UpdateStatement(
    std::string collection_name,
    AssignmentList assignments,
    std::unique_ptr<ExpressionNode> where,
    AstNodeLocation location
) noexcept
    : StatementNode(location)
    , collection_name_(std::move(collection_name))
    , assignments_(std::move(assignments))
    , where_(std::move(where))
{
}

AstNodeKind UpdateStatement::kind() const noexcept
{
    return AstNodeKind::Update;
}

const std::string & UpdateStatement::collection_name() const noexcept
{
    return collection_name_;
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

#include "core/parser/ast/statement/update_statement.hpp"

#include <utility>
#include <cassert>

namespace litedb::core::parser::ast
{

UpdateStatement::UpdateStatement(
    std::string collection_name,
    std::vector<Assignment> assignments,
    std::unique_ptr<ExpressionNode> where,
    AstNodeLocation location
)
    : StatementNode(location)
    , collection_name_(std::move(collection_name))
    , assignments_(std::move(assignments))
    , where_(std::move(where))
{
    assert(!collection_name_.empty());
    assert(!assignments_.empty());
    for (const auto & assignment : assignments_) {
        assert(!assignment.column_name.empty());
        assert(assignment.value != nullptr);
    }
}

AstNodeKind UpdateStatement::kind() const noexcept
{
    return AstNodeKind::Update;
}

const std::string & UpdateStatement::collection_name() const noexcept
{
    return collection_name_;
}

const std::vector<Assignment> & UpdateStatement::assignments() const noexcept
{
    return assignments_;
}

std::optional<const ExpressionNode &> UpdateStatement::where() const noexcept
{
    if (!where_) {
        return std::nullopt;
    }

    return *where_;
}

} // namespace litedb::core::parser::ast

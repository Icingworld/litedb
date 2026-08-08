#include "core/parser/ast/statement/delete_statement.hpp"

#include <cassert>
#include <utility>

namespace litedb::core::parser::ast
{

DeleteStatement::DeleteStatement(
    std::string collection_name,
    std::unique_ptr<ExpressionNode> where,
    AstNodeLocation location
)
    : StatementNode(location)
    , collection_name_(std::move(collection_name))
    , where_(std::move(where))
{
    assert(!collection_name_.empty());
}

AstNodeKind DeleteStatement::kind() const noexcept
{
    return AstNodeKind::Delete;
}

const std::string & DeleteStatement::collection_name() const noexcept
{
    return collection_name_;
}

std::optional<const ExpressionNode &> DeleteStatement::where() const noexcept
{
    if (!where_) {
        return std::nullopt;
    }

    return *where_;
}

} // namespace litedb::core::parser::ast

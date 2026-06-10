#include "core/parser/ast/statement/select_statement.hpp"

#include <utility>

namespace litedb::core::parser::ast
{

SelectStatement::SelectStatement(
    SelectList select_list,
    std::string collection,
    std::unique_ptr<ExpressionNode> where,
    OrderByList order_by,
    std::optional<std::size_t> limit,
    std::optional<std::size_t> offset,
    AstNodeLocation location
) noexcept
    : StatementNode(location)
    , select_list_(std::move(select_list))
    , collection_(std::move(collection))
    , where_(std::move(where))
    , order_by_(std::move(order_by))
    , limit_(limit)
    , offset_(offset)
{
}

AstNodeKind SelectStatement::kind() const noexcept
{
    return AstNodeKind::Select;
}

const SelectStatement::SelectList & SelectStatement::select_list() const noexcept
{
    return select_list_;
}

const std::string & SelectStatement::collection() const noexcept
{
    return collection_;
}

const ExpressionNode * SelectStatement::where() const noexcept
{
    return where_.get();
}

const SelectStatement::OrderByList & SelectStatement::order_by() const noexcept
{
    return order_by_;
}

std::optional<std::size_t> SelectStatement::limit() const noexcept
{
    return limit_;
}

std::optional<std::size_t> SelectStatement::offset() const noexcept
{
    return offset_;
}

} // namespace litedb::core::parser::ast

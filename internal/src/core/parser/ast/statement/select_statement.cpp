#include "core/parser/ast/statement/select_statement.hpp"

#include <cassert>
#include <utility>

namespace litedb::core::parser::ast
{

SelectStatement::SelectStatement(
    std::vector<std::unique_ptr<ExpressionNode>> select_list,
    std::string collection_name,
    std::unique_ptr<ExpressionNode> where,
    std::vector<OrderByItem> order_by,
    std::optional<std::size_t> limit,
    std::optional<std::size_t> offset,
    AstNodeLocation location
)
    : StatementNode(location)
    , select_list_(std::move(select_list))
    , collection_name_(std::move(collection_name))
    , where_(std::move(where))
    , order_by_(std::move(order_by))
    , limit_(limit)
    , offset_(offset)
{
    assert(!select_list_.empty());
    for (const auto & select_item : select_list_) {
        assert(select_item != nullptr);
    }
    assert(!collection_name_.empty());
    for (const auto & order_by_item : order_by_) {
        assert(order_by_item.expression != nullptr);
    }
}

AstNodeKind SelectStatement::kind() const noexcept
{
    return AstNodeKind::Select;
}

const std::vector<std::unique_ptr<ExpressionNode>> & SelectStatement::select_list() const noexcept
{
    return select_list_;
}

const std::string & SelectStatement::collection_name() const noexcept
{
    return collection_name_;
}

std::optional<const ExpressionNode &> SelectStatement::where() const noexcept
{
    if (!where_) {
        return std::nullopt;
    }

    return *where_;
}

const std::vector<OrderByItem> & SelectStatement::order_by() const noexcept
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

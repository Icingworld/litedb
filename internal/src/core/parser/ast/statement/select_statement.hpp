#pragma once

#include <memory>
#include <optional>
#include <string>
#include <vector>

#include "core/parser/ast/expression/expression_node.hpp"
#include "core/parser/ast/statement/statement_node.hpp"

namespace litedb::core::parser::ast
{

struct OrderByItem
{
    std::unique_ptr<ExpressionNode> expression;
    bool ascending {true};
};

class SelectStatement final : public StatementNode
{
public:
    using SelectList = std::vector<std::unique_ptr<ExpressionNode>>;
    using OrderByList = std::vector<OrderByItem>;

    SelectStatement(
        SelectList select_list,
        std::string collection,
        std::unique_ptr<ExpressionNode> where,
        OrderByList order_by,
        std::optional<std::size_t> limit,
        std::optional<std::size_t> offset,
        AstNodeLocation location
    ) noexcept;

    [[nodiscard]]
    AstNodeKind kind() const noexcept override;

    [[nodiscard]]
    const SelectList & select_list() const noexcept;

    [[nodiscard]]
    const std::string & collection() const noexcept;

    [[nodiscard]]
    const ExpressionNode * where() const noexcept;

    [[nodiscard]]
    const OrderByList & order_by() const noexcept;

    [[nodiscard]]
    std::optional<std::size_t> limit() const noexcept;

    [[nodiscard]]
    std::optional<std::size_t> offset() const noexcept;

private:
    SelectList select_list_;
    std::string collection_;
    std::unique_ptr<ExpressionNode> where_;
    OrderByList order_by_;
    std::optional<std::size_t> limit_;
    std::optional<std::size_t> offset_;
};

} // namespace litedb::core::parser::ast

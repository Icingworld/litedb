#pragma once

#include <memory>
#include <optional>
#include <string>
#include <vector>

#include "core/parser/ast/expression/expression_node.hpp"
#include "core/parser/ast/statement/statement_node.hpp"

namespace litedb::core::parser::ast
{

// 排序项
struct OrderByItem
{
    std::unique_ptr<ExpressionNode> expression;     // 表达式
    bool ascending {true};                          // 是否升序
};

// SELECT 语句节点
class SelectStatement final : public StatementNode
{
public:
    SelectStatement(
        std::vector<std::unique_ptr<ExpressionNode>> select_list,
        std::string collection_name,
        std::unique_ptr<ExpressionNode> where,
        std::vector<OrderByItem> order_by,
        std::optional<std::size_t> limit,
        std::optional<std::size_t> offset,
        AstNodeLocation location
    );

public:
    // 获取节点类型
    [[nodiscard]]
    AstNodeKind kind() const noexcept override;

    // 获取选择列表
    [[nodiscard]]
    const std::vector<std::unique_ptr<ExpressionNode>> & select_list() const noexcept;

    // 获取集合名称
    [[nodiscard]]
    const std::string & collection_name() const noexcept;

    // 获取条件表达式
    [[nodiscard]]
    std::optional<const ExpressionNode &> where() const noexcept;

    // 获取排序列表
    [[nodiscard]]
    const std::vector<OrderByItem> & order_by() const noexcept;

    // 获取限制
    [[nodiscard]]
    std::optional<std::size_t> limit() const noexcept;

    // 获取偏移
    [[nodiscard]]
    std::optional<std::size_t> offset() const noexcept;

private:
    std::vector<std::unique_ptr<ExpressionNode>> select_list_;      // 选择列表
    std::string collection_name_;                                   // 集合名称
    std::unique_ptr<ExpressionNode> where_;                         // 条件表达式
    std::vector<OrderByItem> order_by_;                             // 排序列表
    std::optional<std::size_t> limit_;                              // 限制
    std::optional<std::size_t> offset_;                             // 偏移
};

} // namespace litedb::core::parser::ast

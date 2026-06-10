#pragma once

#include <memory>
#include <optional>
#include <string>
#include <vector>

#include "core/parser/ast/expression/expression_node.hpp"
#include "core/parser/ast/statement/statement_node.hpp"

namespace litedb::core::parser::ast
{

/**
 * @brief 排序项
 * @note 示例：expression [ASC | DESC]
 */
struct OrderByItem
{
    std::unique_ptr<ExpressionNode> expression;
    bool ascending {true};
};

/**
 * @brief SELECT 语句节点
 * @details 示例：SELECT <select_item> [, <select_item>] FROM <collection_name> [WHERE <expression>] [ORDER BY <expression> [ASC | DESC]] [LIMIT <integer_literal>] [OFFSET <integer_literal>]
 */
class SelectStatement final : public StatementNode
{
public:
    using SelectList = std::vector<std::unique_ptr<ExpressionNode>>;
    using OrderByList = std::vector<OrderByItem>;

public:
    SelectStatement(
        SelectList select_list,
        std::string collection,
        std::unique_ptr<ExpressionNode> where,
        OrderByList order_by,
        std::optional<std::size_t> limit,
        std::optional<std::size_t> offset,
        AstNodeLocation location
    ) noexcept;

public:
    /**
     * @brief 获取节点类型
     * @return 节点类型
     */
    [[nodiscard]]
    AstNodeKind kind() const noexcept override;

    /**
     * @brief 获取选择列表
     * @return 选择列表
     */
    [[nodiscard]]
    const SelectList & select_list() const noexcept;

    /**
     * @brief 获取集合名称
     * @return 集合名称
     */
    [[nodiscard]]
    const std::string & collection() const noexcept;

    /**
     * @brief 获取条件表达式
     * @return 条件表达式
     */
    [[nodiscard]]
    const ExpressionNode * where() const noexcept;

    /**
     * @brief 获取排序列表
     * @return 排序列表
     */
    [[nodiscard]]
    const OrderByList & order_by() const noexcept;

    /**
     * @brief 获取限制
     * @return 限制
     */
    [[nodiscard]]
    std::optional<std::size_t> limit() const noexcept;

    /**
     * @brief 获取偏移
     * @return 偏移
     */
    [[nodiscard]]
    std::optional<std::size_t> offset() const noexcept;

private:
    SelectList select_list_;                    ///< 选择列表
    std::string collection_;                    ///< 集合名称
    std::unique_ptr<ExpressionNode> where_;     ///< 条件表达式
    OrderByList order_by_;                      ///< 排序列表
    std::optional<std::size_t> limit_;          ///< 限制
    std::optional<std::size_t> offset_;         ///< 偏移
};

} // namespace litedb::core::parser::ast

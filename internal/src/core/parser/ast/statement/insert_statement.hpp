#pragma once

#include <memory>
#include <string>
#include <vector>

#include "core/parser/ast/expression/expression_node.hpp"
#include "core/parser/ast/statement/statement_node.hpp"

namespace litedb::core::parser::ast
{

/**
 * @brief INSERT 语句节点
 * @details 示例：INSERT INTO <collection_name> (<column_name> [, <column_name>]) VALUES (<value> [, <value>])
 */
class InsertStatement final : public StatementNode
{
public:
    using ColumnList = std::vector<std::string>;
    using ValueList = std::vector<std::unique_ptr<ExpressionNode>>;

public:
    InsertStatement(std::string collection, ColumnList columns, ValueList values, AstNodeLocation location) noexcept;

public:
    /**
     * @brief 获取节点类型
     * @return 节点类型
     */
    [[nodiscard]]
    AstNodeKind kind() const noexcept override;

    /**
     * @brief 接受访问器访问
     * @param visitor 访问器
     */
    void accept(AstNodeVisitor & visitor) const override;

    /**
     * @brief 获取集合名称
     * @return 集合名称
     */
    [[nodiscard]]
    const std::string & collection() const noexcept;

    /**
     * @brief 获取列列表
     * @return 列列表
     */
    [[nodiscard]]
    const ColumnList & columns() const noexcept;

    /**
     * @brief 获取值列表
     * @return 值列表
     */
    [[nodiscard]]
    const ValueList & values() const noexcept;

private:
    std::string collection_;        ///< 集合名称
    ColumnList columns_;            ///< 列列表
    ValueList values_;              ///< 值列表
};

} // namespace litedb::core::parser::ast

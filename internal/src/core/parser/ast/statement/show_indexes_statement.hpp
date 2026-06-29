#pragma once

#include <string>

#include "core/parser/ast/statement/statement_node.hpp"

namespace litedb::core::parser::ast
{

/**
 * @brief SHOW INDEXES 语句节点
 * @details 示例：SHOW INDEXES FROM <collection_name>
 */
class ShowIndexesStatement final : public StatementNode
{
public:
    ShowIndexesStatement(std::string collection_name, AstNodeLocation location) noexcept;

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
    const std::string & collection_name() const noexcept;

private:
    std::string collection_name_;    ///< 集合名称
};

} // namespace litedb::core::parser::ast

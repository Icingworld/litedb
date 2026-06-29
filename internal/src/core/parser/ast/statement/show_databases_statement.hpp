#pragma once

#include "core/parser/ast/statement/statement_node.hpp"

namespace litedb::core::parser::ast
{

/**
 * @brief SHOW DATABASES 语句节点
 * @details 示例：SHOW DATABASES
 */
class ShowDatabasesStatement final : public StatementNode
{
public:
    explicit ShowDatabasesStatement(AstNodeLocation location) noexcept;

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
};

} // namespace litedb::core::parser::ast

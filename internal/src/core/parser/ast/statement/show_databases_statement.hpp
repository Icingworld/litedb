#pragma once

#include "core/parser/ast/statement/statement_node.hpp"

namespace litedb::core::parser::ast
{

// SHOW DATABASES 语句节点
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
};

} // namespace litedb::core::parser::ast

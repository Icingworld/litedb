#pragma once

#include <string>

#include "core/parser/ast/statement/statement_node.hpp"

namespace litedb::core::parser::ast
{

// USE 语句节点
class UseStatement final : public StatementNode
{
public:
    UseStatement(std::string database_name, AstNodeLocation location);

public:
    // 获取节点类型
    [[nodiscard]]
    AstNodeKind kind() const noexcept override;

    // 获取数据库名称
    [[nodiscard]]
    const std::string & database_name() const noexcept;

private:
    std::string database_name_;
};

} // namespace litedb::core::parser::ast

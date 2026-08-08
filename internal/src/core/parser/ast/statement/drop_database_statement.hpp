#pragma once

#include <string>

#include "core/parser/ast/statement/statement_node.hpp"

namespace litedb::core::parser::ast
{

// DROP DATABASE 语句节点
class DropDatabaseStatement final : public StatementNode
{
public:
    DropDatabaseStatement(
        std::string database_name,
        bool if_exists,
        AstNodeLocation location
    );

public:
    // 获取节点类型
    [[nodiscard]]
    AstNodeKind kind() const noexcept override;

    // 获取数据库名称
    [[nodiscard]]
    const std::string & database_name() const noexcept;

    // 是否存在 IF EXISTS
    [[nodiscard]]
    bool if_exists() const noexcept;

private:
    std::string database_name_;
    bool if_exists_;
};

} // namespace litedb::core::parser::ast

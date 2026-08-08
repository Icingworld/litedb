#pragma once

#include <string>

#include "core/parser/ast/statement/statement_node.hpp"

namespace litedb::core::parser::ast
{

// CREATE DATABASE 语句节点
class CreateDatabaseStatement final : public StatementNode
{
public:
    CreateDatabaseStatement(
        std::string database_name,
        bool if_not_exists,
        AstNodeLocation location
    );

public:
    // 获取节点类型
    [[nodiscard]]
    AstNodeKind kind() const noexcept override;

    // 获取数据库名称
    [[nodiscard]]
    const std::string & database_name() const noexcept;

    // 是否存在 IF NOT EXISTS
    [[nodiscard]]
    bool if_not_exists() const noexcept;

private:
    std::string database_name_;     // 数据库名称
    bool if_not_exists_;            // 是否存在 IF NOT EXISTS
};

} // namespace litedb::core::parser::ast

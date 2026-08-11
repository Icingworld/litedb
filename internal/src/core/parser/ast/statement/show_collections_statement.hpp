#pragma once

#include <optional>
#include <string>

#include "core/parser/ast/statement/statement_node.hpp"

namespace litedb::core::parser::ast
{

// SHOW COLLECTIONS 语句节点
class ShowCollectionsStatement final : public StatementNode
{
public:
    ShowCollectionsStatement(std::optional<std::string> database_name, AstNodeLocation location);

public:
    // 获取节点类型
    [[nodiscard]]
    AstNodeKind kind() const noexcept override;

    // 获取数据库名称
    [[nodiscard]]
    std::optional<const std::string &> database_name() const noexcept;

private:
    std::optional<std::string> database_name_;
};

} // namespace litedb::core::parser::ast

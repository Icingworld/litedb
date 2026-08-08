#pragma once

#include <optional>
#include <string>

#include "core/parser/ast/statement/statement_node.hpp"
#include "core/parser/ast/column_definition.hpp"

namespace litedb::core::parser::ast
{

// CREATE COLLECTION 语句节点
class CreateCollectionStatement final : public StatementNode
{
public:
    CreateCollectionStatement(
        std::string collection_name,
        bool if_not_exists,
        std::vector<std::unique_ptr<ColumnDefinitionSyntax>> columns,
        std::optional<std::string> comment,
        AstNodeLocation location
    );

public:
    // 获取节点类型
    [[nodiscard]]
    AstNodeKind kind() const noexcept override;

    // 获取集合名称
    [[nodiscard]]
    const std::string & collection_name() const noexcept;

    // 是否存在 IF NOT EXISTS
    [[nodiscard]]
    bool if_not_exists() const noexcept;

    // 获取列定义列表
    [[nodiscard]]
    const std::vector<std::unique_ptr<ColumnDefinitionSyntax>> & columns() const noexcept;

    // 获取集合注释
    [[nodiscard]]
    const std::optional<std::string> & comment() const noexcept;

private:
    std::string collection_name_;                                   // 集合名称
    bool if_not_exists_;                                            // 是否存在 IF NOT EXISTS
    std::vector<std::unique_ptr<ColumnDefinitionSyntax>> columns_;  // 列定义列表
    std::optional<std::string> comment_;                            // 集合注释
};

} // namespace litedb::core::parser::ast

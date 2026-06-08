#pragma once

#include <string>

#include "core/parser/ast/schema.hpp"
#include "core/parser/ast/statement/statement_node.hpp"

namespace litedb::core::parser::ast
{

/**
 * @brief CREATE COLLECTION 语句节点
 * @details 示例：CREATE COLLECTION [IF NOT EXISTS] <collection_name> (<column_definition> [, <column_definition>])
 */
class CreateCollectionStatement final : public StatementNode
{
public:
    CreateCollectionStatement(
        std::string collection,
        bool if_not_exists,
        ColumnDefinitionList columns,
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
     * @brief 获取集合名称
     * @return 集合名称
     */
    [[nodiscard]]
    const std::string & collection() const noexcept;

    /**
     * @brief 是否存在
     * @return 是否存在
     */
    [[nodiscard]]
    bool if_not_exists() const noexcept;

    /**
     * @brief 获取列定义列表
     * @return 列定义列表
     */
    [[nodiscard]]
    const ColumnDefinitionList & columns() const noexcept;

private:
    std::string collection_;        ///< 集合名称
    bool if_not_exists_;            ///< 是否不存在
    ColumnDefinitionList columns_;  ///< 列定义列表
};

} // namespace litedb::core::parser::ast

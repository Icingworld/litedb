#pragma once

#include <optional>
#include <string>

#include "core/parser/ast/column_definition.hpp"
#include "core/parser/ast/statement/statement_node.hpp"

namespace litedb::core::parser::ast
{

/**
 * @brief CREATE COLLECTION 语句节点
 */
class CreateCollectionStatement final : public StatementNode
{
public:
    CreateCollectionStatement(
        std::string collection_name,
        bool if_not_exists,
        ColumnDefinitionSyntaxList columns,
        std::optional<std::string> comment,
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
    const std::string & collection_name() const noexcept;

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
    const ColumnDefinitionSyntaxList & columns() const noexcept;

    /**
     * @brief 获取集合注释
     * @return 集合注释
     */
    [[nodiscard]]
    const std::optional<std::string> & comment() const noexcept;

private:
    std::string collection_name_;               ///< 集合名称
    bool if_not_exists_;                        ///< 是否不存在
    ColumnDefinitionSyntaxList columns_;        ///< 列定义列表
    std::optional<std::string> comment_;        ///< 集合注释
};

} // namespace litedb::core::parser::ast

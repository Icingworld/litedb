#pragma once

#include <optional>
#include <string>

#include "core/parser/ast/statement/statement_node.hpp"

namespace litedb::core::parser::ast
{

/**
 * @brief SHOW COLLECTIONS 语句节点
 */
class ShowCollectionsStatement final : public StatementNode
{
public:
    ShowCollectionsStatement(
        std::optional<std::string> database_name,
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
     * @brief 获取数据库名称
     * @return 数据库名称
     */
    [[nodiscard]]
    const std::optional<std::string> & database_name() const noexcept;

private:
    std::optional<std::string> database_name_;    // 数据库名称
};

} // namespace litedb::core::parser::ast

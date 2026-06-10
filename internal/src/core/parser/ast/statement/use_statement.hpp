#pragma once

#include <string>

#include "core/parser/ast/statement/statement_node.hpp"

namespace litedb::core::parser::ast
{

/**
 * @brief USE 语句节点
 * @details 示例：USE <database_name>
 */
class UseStatement final : public StatementNode
{
public:
    UseStatement(std::string database, AstNodeLocation location) noexcept;

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
    const std::string & database() const noexcept;

private:
    std::string database_;      ///< 数据库名称
};

} // namespace litedb::core::parser::ast

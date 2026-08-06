#pragma once

#include <string>

#include "core/parser/ast/statement/statement_node.hpp"

namespace litedb::core::parser::ast
{

/**
 * @brief CREATE DATABASE 语句节点
 */
class CreateDatabaseStatement final : public StatementNode
{
public:
    CreateDatabaseStatement(
        std::string database_name,
        bool if_not_exists,
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
    const std::string & database_name() const noexcept;

    /**
     * @brief 是否存在
     * @return 是否存在
     */
    [[nodiscard]]
    bool if_not_exists() const noexcept;

private:
    std::string database_name_; ///< 数据库名称
    bool if_not_exists_;        ///< 是否存在
};

} // namespace litedb::core::parser::ast

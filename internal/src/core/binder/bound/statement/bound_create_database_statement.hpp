#pragma once

#include <string>

#include "core/binder/bound/statement/bound_statement.hpp"

namespace litedb::core::binder::bound
{

/**
 * @brief CREATE DATABASE 语句节点
 * @details 示例：CREATE DATABASE [IF NOT EXISTS] database_name
 */
class BoundCreateDatabaseStatement final : public BoundStatement
{
public:
    BoundCreateDatabaseStatement(std::string database_name, bool if_not_exists, parser::ast::AstNodeLocation location);

public:
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

    /**
     * @brief 接受访问器访问
     * @param visitor 访问器
     */
    void accept(BoundStatementVisitor & visitor) const override;

private:
    std::string database_name_;         ///< 数据库名称
    bool if_not_exists_;                ///< 是否不存在
};

} // namespace litedb::core::binder::bound

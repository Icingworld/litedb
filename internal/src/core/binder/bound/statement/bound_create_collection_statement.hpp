#pragma once

#include <optional>
#include <string>
#include <vector>

#include "core/binder/bound/statement/bound_statement.hpp"
#include "core/meta/meta.hpp"

namespace litedb::core::binder::bound
{

/**
 * @brief CREATE COLLECTION 语句节点
 * @details 示例：CREATE COLLECTION [IF NOT EXISTS] collection_name (<column_definition> [, <column_definition>])
 */
class BoundCreateCollectionStatement final : public BoundStatement
{
public:
    BoundCreateCollectionStatement(
        common::DatabaseId database_id,
        std::string collection_name,
        bool if_not_exists,
        std::vector<meta::ColumnDefinition> columns,
        std::optional<std::string> comment,
        parser::ast::AstNodeLocation location
    );

public:
    /**
     * @brief 获取数据库 ID
     * @return 数据库 ID
     */
    [[nodiscard]]
    common::DatabaseId database_id() const noexcept;

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
    const std::vector<meta::ColumnDefinition> & columns() const noexcept;

    /**
     * @brief 获取集合注释
     * @return 集合注释
     */
    [[nodiscard]]
    const std::optional<std::string> & comment() const noexcept;

    /**
     * @brief 接受访问器访问
     * @param visitor 访问器
     */
    void accept(BoundStatementVisitor & visitor) const override;

private:
    common::DatabaseId database_id_;                    ///< 数据库 ID
    std::string collection_name_;                       ///< 集合名称
    bool if_not_exists_;                                ///< 是否不存在
    std::vector<meta::ColumnDefinition> columns_;    ///< 列定义列表
    std::optional<std::string> comment_;                ///< 集合注释
};

} // namespace litedb::core::binder::bound

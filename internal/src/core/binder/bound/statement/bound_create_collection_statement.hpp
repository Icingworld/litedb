#pragma once

#include <optional>
#include <string>
#include <vector>

#include "core/binder/bound/statement/bound_statement.hpp"
#include "core/meta/meta_request.hpp"

namespace litedb::core::binder::bound
{

/**
 * @brief 绑定 CREATE COLLECTION 语句
 */
class BoundCreateCollectionStatement final : public BoundStatement
{
public:
    BoundCreateCollectionStatement(
        common::DatabaseId database_id,
        std::optional<std::string> collection_name,
        std::vector<meta::ColumnDefinition> columns,
        std::optional<std::string> comment
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
    const std::optional<std::string> & collection_name() const noexcept;

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

private:
    common::DatabaseId database_id_;                     // 数据库 ID
    std::optional<std::string> collection_name_;         // 集合名称
    std::vector<meta::ColumnDefinition> columns_;        // 列定义列表
    std::optional<std::string> comment_;                 // 集合注释
};

} // namespace litedb::core::binder::bound

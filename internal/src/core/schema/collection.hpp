#pragma once

#include <optional>
#include <string>
#include <string_view>
#include <vector>

#include "core/common/ids.hpp"
#include "core/schema/column.hpp"

namespace litedb::core::schema
{

/**
 * @brief 集合 schema
 */
class CollectionSchema
{
public:
    CollectionSchema(
        common::DatabaseId database_id,
        common::CollectionId collection_id,
        std::string collection_name,
        std::vector<ColumnSchema> columns,
        std::optional<std::string> comment = std::nullopt
    );

public:
    /**
     * @brief 获取数据库 ID
     * @return 数据库 ID
     */
    [[nodiscard]]
    common::DatabaseId database_id() const noexcept;

    /**
     * @brief 获取集合 ID
     * @return 集合 ID
     */
    [[nodiscard]]
    common::CollectionId collection_id() const noexcept;

    /**
     * @brief 获取集合名称
     * @return 集合名称
     */
    [[nodiscard]]
    const std::string & collection_name() const noexcept;

    /**
     * @brief 获取集合注释
     * @return 集合注释
     */
    [[nodiscard]]
    const std::optional<std::string> & comment() const noexcept;

    /**
     * @brief 获取列 schema
     * @return 列 schema
     */
    [[nodiscard]]
    const std::vector<ColumnSchema> & columns() const noexcept;

    /**
     * @brief 获取列 schema 在指定位置的指针
     * @param ordinal 列序号
     * @return 列 schema 的指针
     */
    [[nodiscard]]
    const ColumnSchema * column_at(std::size_t ordinal) const noexcept;

    /**
     * @brief 获取列 schema 在指定列 ID 的指针
     * @param column_id 列 ID
     * @return 列 schema 的指针
     */
    [[nodiscard]]
    const ColumnSchema * find_column(common::ColumnId column_id) const noexcept;

    /**
     * @brief 获取列 schema 在指定列名称的指针
     * @param column_name 列名称
     * @return 列 schema 的指针
     */
    [[nodiscard]]
    const ColumnSchema * find_column(std::string_view column_name) const;

private:
    common::DatabaseId database_id_;                    // 数据库 ID
    common::CollectionId collection_id_;                // 集合 ID
    std::string collection_name_;                       // 集合名称
    std::vector<ColumnSchema> columns_;                 // 列 schema
    std::optional<std::string> comment_;                // 集合注释
};

} // namespace litedb::core::schema

#pragma once

#include <optional>
#include <string>
#include <string_view>
#include <vector>

#include "core/common/ids.hpp"
#include "core/schema/column.hpp"

namespace litedb::core::schema
{

// 集合模型
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
    // 获取数据库 ID
    [[nodiscard]]
    common::DatabaseId database_id() const noexcept;

    // 获取集合 ID
    [[nodiscard]]
    common::CollectionId collection_id() const noexcept;

    // 获取集合名称
    [[nodiscard]]
    const std::string & collection_name() const noexcept;

    // 获取集合注释
    [[nodiscard]]
    const std::optional<std::string> & comment() const noexcept;

    // 获取列 schema
    [[nodiscard]]
    const std::vector<ColumnSchema> & columns() const noexcept;

    // 获取列 schema 在指定位置的列
    [[nodiscard]]
    std::optional<const ColumnSchema &> column_at(std::size_t ordinal) const noexcept;

    // 获取列 schema 在指定列 ID 的列
    [[nodiscard]]
    std::optional<const ColumnSchema &> find_column(common::ColumnId column_id) const noexcept;

    // 获取列 schema 在指定列名称的列
    [[nodiscard]]
    std::optional<const ColumnSchema &> find_column(std::string_view column_name) const;

private:
    common::DatabaseId database_id_;
    common::CollectionId collection_id_;
    std::string collection_name_;
    std::vector<ColumnSchema> columns_;
    std::optional<std::string> comment_;
};

} // namespace litedb::core::schema

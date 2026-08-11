#pragma once

#include <optional>
#include <string>

#include "core/common/ids.hpp"
#include "core/common/logical_type.hpp"
#include "core/schema/default_expression.hpp"

namespace litedb::core::schema
{

// 列模型
class ColumnSchema
{
public:
    ColumnSchema(
        common::ColumnId column_id,
        common::CollectionId collection_id,
        std::size_t ordinal,
        std::string column_name,
        common::LogicalType type,
        bool nullable,
        bool unique,
        std::optional<DefaultExpression> default_expression,
        std::optional<std::string> comment
    );

public:
    // 获取列 ID
    [[nodiscard]]
    common::ColumnId column_id() const noexcept;

    // 获取集合 ID
    [[nodiscard]]
    common::CollectionId collection_id() const noexcept;

    // 获取列序号
    [[nodiscard]]
    std::size_t ordinal() const noexcept;

    // 获取列名称
    [[nodiscard]]
    const std::string & column_name() const noexcept;

    // 获取列类型
    [[nodiscard]]
    const common::LogicalType & type() const noexcept;

    // 是否可为空
    [[nodiscard]]
    bool nullable() const noexcept;

    // 是否唯一
    [[nodiscard]]
    bool unique() const noexcept;

    // 获取默认值
    [[nodiscard]]
    const std::optional<DefaultExpression> & default_expression() const noexcept;

    // 获取注释
    [[nodiscard]]
    std::optional<const std::string &> comment() const noexcept;

private:
    common::ColumnId column_id_;
    common::CollectionId collection_id_;
    std::size_t ordinal_;
    std::string column_name_;
    common::LogicalType type_;
    bool nullable_;
    bool unique_;
    std::optional<DefaultExpression> default_expression_;
    std::optional<std::string> comment_;
};

} // namespace litedb::core::schema

#pragma once

#include <cstddef>
#include <optional>
#include <string>

#include "core/catalog/entry/catalog_entry.hpp"
#include "core/common/ids.hpp"
#include "core/common/logical_type.hpp"
#include "core/schema/default_expression.hpp"

namespace litedb::core::catalog::entry
{

// 列项
class ColumnEntry final : public CatalogEntry
{
public:
    ColumnEntry(
        common::ColumnId id,
        common::CollectionId collection_id,
        std::size_t ordinal,
        std::string name,
        common::LogicalType type,
        bool unique,
        bool nullable,
        std::optional<schema::DefaultExpression> default_expression = std::nullopt,
        std::optional<std::string> comment = std::nullopt
    );

public:
    // 获取列 ID
    [[nodiscard]]
    common::ColumnId id() const noexcept;

    // 获取集合 ID
    [[nodiscard]]
    common::CollectionId collection_id() const noexcept;

    // 获取列序号
    [[nodiscard]]
    std::size_t ordinal() const noexcept;

    // 获取列类型
    [[nodiscard]]
    const common::LogicalType & type() const noexcept;

    // 是否唯一
    [[nodiscard]]
    bool unique() const noexcept;

    // 是否可为空
    [[nodiscard]]
    bool nullable() const noexcept;

    // 获取默认值表达式
    [[nodiscard]]
    std::optional<const schema::DefaultExpression &> default_expression() const noexcept;

    // 获取列注释
    [[nodiscard]]
    std::optional<const std::string &> comment() const noexcept;

private:
    common::CollectionId collection_id_;
    std::size_t ordinal_;
    common::LogicalType type_;
    bool unique_;
    bool nullable_;
    std::optional<schema::DefaultExpression> default_expression_;
    std::optional<std::string> comment_;
};

} // namespace litedb::core::catalog::entry

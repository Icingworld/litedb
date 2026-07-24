#pragma once

#include <cstddef>
#include <optional>
#include <string>

#include "core/common/ids.hpp"
#include "core/common/logical_type.hpp"
#include "core/schema/default_expression.hpp"
#include "core/meta/entry/meta_entry.hpp"

namespace litedb::core::meta::entry
{

/**
 * @brief 列项
 */
class ColumnEntry final : public MetaEntry
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
    /**
     * @brief 获取列 ID
     * @return 列 ID
     */
    [[nodiscard]]
    common::ColumnId id() const noexcept;

    /**
     * @brief 获取集合 ID
     * @return 集合 ID
     */
    [[nodiscard]]
    common::CollectionId collection_id() const noexcept;

    /**
     * @brief 获取列序号
     * @return 列序号
     */
    [[nodiscard]]
    std::size_t ordinal() const noexcept;

    /**
     * @brief 获取列类型
     * @return 列类型
     */
    [[nodiscard]]
    const common::LogicalType & type() const noexcept;

    /**
     * @brief 是否唯一
     * @return 是否唯一
     */
    [[nodiscard]]
    bool unique() const noexcept;

    /**
     * @brief 是否可为空
     * @return 是否可为空
     */
    [[nodiscard]]
    bool nullable() const noexcept;

    /**
     * @brief 获取默认值表达式
     * @return 默认值表达式
     */
    [[nodiscard]]
    const std::optional<schema::DefaultExpression> & default_expression() const noexcept;

    /**
     * @brief 获取列注释
     * @return 列注释
     */
    [[nodiscard]]
    const std::optional<std::string> & comment() const noexcept;

private:
    common::CollectionId collection_id_;                    ///< 集合 ID
    std::size_t ordinal_;                                   ///< 列序号
    common::LogicalType type_;                              ///< 列类型
    bool unique_;                                           ///< 是否唯一
    bool nullable_;                                         ///< 是否可为空
    std::optional<schema::DefaultExpression> default_expression_;   ///< 默认值表达式
    std::optional<std::string> comment_;                    ///< 列注释
};

} // namespace litedb::core::meta::entry

#pragma once

#include <optional>
#include <string>

#include "core/meta/entry/default_expression.hpp"
#include "core/common/ids.hpp"
#include "core/common/logical_type.hpp"

namespace litedb::core::schema
{

/**
 * @brief 列 schema
 */
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
        std::optional<meta::entry::DefaultExpression> default_expression,
        std::optional<std::string> comment
    );

public:
    /**
     * @brief 获取列 ID
     * @return 列 ID
     */
    [[nodiscard]]
    common::ColumnId column_id() const noexcept;

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
     * @brief 获取列名称
     * @return 列名称
     */
    [[nodiscard]]
    const std::string & column_name() const noexcept;

    /**
     * @brief 获取列类型
     * @return 列类型
     */
    [[nodiscard]]
    const common::LogicalType & type() const noexcept;

    /**
     * @brief 是否可为空
     * @return 是否可为空
     */
    [[nodiscard]]
    bool nullable() const noexcept;

    /**
     * @brief 是否唯一
     * @return 是否唯一
     */
    [[nodiscard]]
    bool unique() const noexcept;

    /**
     * @brief 获取默认值
     * @return 默认值
     */
    [[nodiscard]]
    const std::optional<meta::entry::DefaultExpression> & default_expression() const noexcept;

    /**
     * @brief 获取注释
     * @return 注释
     */
    [[nodiscard]]
    const std::optional<std::string> & comment() const noexcept;

private:
    common::ColumnId column_id_;                                            ///< 列 ID
    common::CollectionId collection_id_;                                    ///< 集合 ID
    std::size_t ordinal_;                                                   ///< 列序号
    std::string column_name_;                                               ///< 列名称
    common::LogicalType type_;                                              ///< 列类型
    bool nullable_;                                                         ///< 是否可为空
    bool unique_;                                                           ///< 是否唯一
    std::optional<meta::entry::DefaultExpression> default_expression_;   ///< 默认值
    std::optional<std::string> comment_;                                    ///< 注释
};

} // namespace litedb::core::schema

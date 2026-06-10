#pragma once

#include <string>

#include "core/binder/bound/expression/bound_expression.hpp"
#include "core/common/ids.hpp"

namespace litedb::core::binder::bound
{

/**
 * @brief 列引用表达式节点
 * @details 示例：collection.column
 */
class BoundColumnRefExpression final : public BoundExpression
{
public:
    BoundColumnRefExpression(
        common::DatabaseId database_id,
        common::CollectionId collection_id,
        std::string collection_name,
        common::ColumnId column_id,
        std::string column_name,
        common::LogicalType type,
        bool nullable,
        parser::ast::AstNodeLocation location
    );

public:
    /**
     * @brief 获取数据库ID
     * @return 数据库ID
     */
    [[nodiscard]]
    common::DatabaseId database_id() const noexcept;

    /**
     * @brief 获取集合ID
     * @return 集合ID
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
     * @brief 获取列ID
     * @return 列ID
     */
    [[nodiscard]]
    common::ColumnId column_id() const noexcept;

    /**
     * @brief 获取列名称
     * @return 列名称
     */
    [[nodiscard]]
    const std::string & column_name() const noexcept;

    /**
     * @brief 是否可为空
     * @return 是否可为空
     */
    [[nodiscard]]
    bool nullable() const noexcept;

private:
    common::DatabaseId database_id_;            ///< 数据库ID
    common::CollectionId collection_id_;        ///< 集合ID
    std::string collection_name_;               ///< 集合名称
    common::ColumnId column_id_;                ///< 列ID
    std::string column_name_;                   ///< 列名称
    bool nullable_;                             ///< 是否可为空
};

} // namespace litedb::core::binder::bound

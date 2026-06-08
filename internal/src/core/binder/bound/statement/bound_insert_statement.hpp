#pragma once

#include <memory>
#include <string>
#include <vector>

#include "core/binder/bound/expression/bound_expression.hpp"
#include "core/binder/bound/statement/bound_statement.hpp"

namespace litedb::core::binder::bound
{

/**
 * @brief INSERT 语句节点
 * @details 示例：INSERT INTO collection_name (column1, column2, ...) VALUES (value1, value2, ...)
 */
class BoundInsertStatement final : public BoundStatement
{
public:
    BoundInsertStatement(
        common::DatabaseId database_id,
        common::CollectionId collection_id,
        std::string collection_name,
        std::vector<BoundColumn> columns,
        std::vector<std::unique_ptr<BoundExpression>> values,
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
     * @brief 获取列列表
     * @return 列列表
     */
    [[nodiscard]]
    const std::vector<BoundColumn> & columns() const noexcept;

    /**
     * @brief 获取值列表
     * @return 值列表
     */
    [[nodiscard]]
    const std::vector<std::unique_ptr<BoundExpression>> & values() const noexcept;

private:
    common::DatabaseId database_id_;                            ///< 数据库ID
    common::CollectionId collection_id_;                        ///< 集合ID
    std::string collection_name_;                               ///< 集合名称
    std::vector<BoundColumn> columns_;                          ///< 列列表
    std::vector<std::unique_ptr<BoundExpression>> values_;      ///< 值列表
};

} // namespace litedb::core::binder::bound

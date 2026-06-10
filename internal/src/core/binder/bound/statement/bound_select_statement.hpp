#pragma once

#include <memory>
#include <optional>
#include <string>
#include <vector>

#include "core/binder/bound/expression/bound_expression.hpp"
#include "core/binder/bound/statement/bound_statement.hpp"

namespace litedb::core::binder::bound
{

/**
 * @brief 排序项
 * @note 示例：expression [ASC | DESC]
 */
struct BoundOrderByItem
{
    std::unique_ptr<BoundExpression> expression;
    bool ascending {true};
};

/**
 * @brief SELECT 语句节点
 * @details 示例：SELECT <select_item> [, <select_item>] FROM <collection_name> [WHERE <expression>] [ORDER BY <expression> [ASC | DESC]] [LIMIT <integer_literal>] [OFFSET <integer_literal>]
 */
class BoundSelectStatement final : public BoundStatement
{
public:
    BoundSelectStatement(
        common::DatabaseId database_id,
        common::CollectionId collection_id,
        std::string collection_name,
        std::vector<std::unique_ptr<BoundExpression>> projections,
        std::unique_ptr<BoundExpression> where,
        std::vector<BoundOrderByItem> order_by,
        std::optional<std::size_t> limit,
        std::optional<std::size_t> offset,
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
     * @brief 获取选择列表
     * @return 选择列表
     */
    [[nodiscard]]
    const std::vector<std::unique_ptr<BoundExpression>> & projections() const noexcept;

    /**
     * @brief 获取条件表达式
     * @return 条件表达式
     */
    [[nodiscard]]
    const BoundExpression * where() const noexcept;

    /**
     * @brief 获取排序列表
     * @return 排序列表
     */
    [[nodiscard]]
    const std::vector<BoundOrderByItem> & order_by() const noexcept;

    /**
     * @brief 获取限制
     * @return 限制
     */
    [[nodiscard]]
    std::optional<std::size_t> limit() const noexcept;

    /**
     * @brief 获取偏移
     * @return 偏移
     */
    [[nodiscard]]
    std::optional<std::size_t> offset() const noexcept;

    [[nodiscard]]
    std::vector<std::unique_ptr<BoundExpression>> take_projections() noexcept;

    [[nodiscard]]
    std::unique_ptr<BoundExpression> take_where() noexcept;

    [[nodiscard]]
    std::vector<BoundOrderByItem> take_order_by() noexcept;

private:
    common::DatabaseId database_id_;                                ///< 数据库ID
    common::CollectionId collection_id_;                            ///< 集合ID
    std::string collection_name_;                                   ///< 集合名称
    std::vector<std::unique_ptr<BoundExpression>> projections_;     ///< 选择列表
    std::unique_ptr<BoundExpression> where_;                        ///< 条件表达式
    std::vector<BoundOrderByItem> order_by_;                        ///< 排序列表
    std::optional<std::size_t> limit_;                              ///< 限制
    std::optional<std::size_t> offset_;                             ///< 偏移
};

} // namespace litedb::core::binder::bound

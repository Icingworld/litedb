#pragma once

#include <memory>
#include <optional>
#include <string>
#include <vector>

#include "core/binder/bound/statement/bound_statement.hpp"
#include "core/binder/bound/expression/bound_expression.hpp"
#include "core/common/ids.hpp"

namespace litedb::core::binder::bound
{

/**
 * @brief 排序项
 */
struct BoundOrderByItem
{
    std::unique_ptr<BoundExpression> expression;    ///< 排序表达式
    bool ascending {true};                          ///< 是否升序
};

/**
 * @brief 投影项
 */
struct BoundProjectionItem
{
    std::unique_ptr<BoundExpression> expression;    ///< 投影表达式
    std::string output_name;                        ///< 投影输出名称
};

/**
 * @brief 绑定 SELECT 语句
 */
class BoundSelectStatement final : public BoundStatement
{
public:
    BoundSelectStatement(
        common::CollectionId collection_id,
        std::vector<BoundProjectionItem> projections,
        std::unique_ptr<BoundExpression> where,
        std::vector<BoundOrderByItem> order_by,
        std::optional<std::size_t> limit,
        std::optional<std::size_t> offset
    );

public:
    /**
     * @brief 获取集合 ID
     * @return 集合 ID
     */
    [[nodiscard]]
    common::CollectionId collection_id() const noexcept;

    /**
     * @brief 获取选择列表
     * @return 选择列表
     */
    [[nodiscard]]
    const std::vector<BoundProjectionItem> & projections() const noexcept;

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

    /**
     * @brief 获取选择列表
     * @return 选择列表
     */
    [[nodiscard]]
    std::vector<BoundProjectionItem> take_projections() noexcept;

    /**
     * @brief 获取条件表达式
     * @return 条件表达式
     */
    [[nodiscard]]
    std::unique_ptr<BoundExpression> take_where() noexcept;

    /**
     * @brief 获取排序列表
     * @return 排序列表
     */
    [[nodiscard]]
    std::vector<BoundOrderByItem> take_order_by() noexcept;

private:
    common::CollectionId collection_id_;                            ///< 集合 ID
    std::vector<BoundProjectionItem> projections_;                  ///< 选择列表
    std::unique_ptr<BoundExpression> where_;                        ///< 条件表达式
    std::vector<BoundOrderByItem> order_by_;                        ///< 排序列表
    std::optional<std::size_t> limit_;                              ///< 限制
    std::optional<std::size_t> offset_;                             ///< 偏移
};

} // namespace litedb::core::binder::bound

#pragma once

#include <cstddef>
#include <functional>
#include <memory>
#include <optional>

#include "core/binder/bound/expression/bound_expression.hpp"
#include "core/common/ids.hpp"
#include "core/meta/entry/vector_index_entry.hpp"
#include "core/physical_planner/operator/physical_operator.hpp"

namespace litedb::core::physical_planner::op
{

/**
 * @brief 向量检索算子
 */
class VectorSearchOperator final : public PhysicalOperator
{
public:
    VectorSearchOperator(
        common::CollectionId collection_id,
        common::VIndexId index_id,
        common::ColumnId column_id,
        meta::entry::VectorDistanceMetric metric,
        std::unique_ptr<binder::bound::BoundExpression> query_vector,
        std::unique_ptr<binder::bound::BoundExpression> predicate,
        std::size_t required_count
    ) noexcept;

public:
    /**
     * @brief 获取集合 ID
     * @return 集合 ID
     */
    [[nodiscard]]
    common::CollectionId collection_id() const noexcept;

    /**
     * @brief 获取向量索引 ID
     * @return 向量索引 ID
     */
    [[nodiscard]]
    common::VIndexId index_id() const noexcept;

    /**
     * @brief 获取列 ID
     * @return 列 ID
     */
    [[nodiscard]]
    common::ColumnId column_id() const noexcept;

    /**
     * @brief 获取距离度量
     * @return 距离度量
     */
    [[nodiscard]]
    meta::entry::VectorDistanceMetric metric() const noexcept;

    /**
     * @brief 获取查询向量表达式
     * @return 查询向量表达式
     */
    [[nodiscard]]
    const binder::bound::BoundExpression & query_vector() const noexcept;

    /**
     * @brief 获取可选残差谓词
     * @return 残差谓词，不存在则为 nullopt
     */
    [[nodiscard]]
    std::optional<std::reference_wrapper<const binder::bound::BoundExpression>>
    predicate() const noexcept;

    /**
     * @brief 获取所需结果数量
     * @return 所需结果数量
     */
    [[nodiscard]]
    std::size_t required_count() const noexcept;

private:
    common::CollectionId collection_id_;                                    ///< 集合 ID
    common::VIndexId index_id_;                                             ///< 向量索引 ID
    common::ColumnId column_id_;                                            ///< 列 ID
    meta::entry::VectorDistanceMetric metric_;                              ///< 距离度量
    std::unique_ptr<binder::bound::BoundExpression> query_vector_;          ///< 查询向量
    std::unique_ptr<binder::bound::BoundExpression> predicate_;             ///< 可选残差谓词
    std::size_t required_count_;                                            ///< 所需结果数量
};

} // namespace litedb::core::physical_planner::op

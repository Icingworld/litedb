#pragma once

#include <cstddef>
#include <memory>
#include <string>

#include "core/binder/bound/expression/bound_expression.hpp"
#include "core/common/ids.hpp"
#include "core/meta/entry/vector_index_entry.hpp"
#include "core/logical_plan/node/logical_plan_node.hpp"

namespace litedb::core::planner::logical
{

/**
 * @brief 向量索引候选搜索节点
 * @details 使用 HNSW 获取候选记录，可在节点内应用 WHERE 谓词。
 */
class LogicalVectorSearch final : public LogicalPlanNode
{
public:
    LogicalVectorSearch(
        common::DatabaseId database_id,
        common::CollectionId collection_id,
        std::string collection_name,
        common::VIndexId index_id,
        std::string index_name,
        common::ColumnId column_id,
        std::string column_name,
        meta::entry::VectorDistanceMetric metric,
        std::unique_ptr<binder::bound::BoundExpression> query_vector,
        std::unique_ptr<binder::bound::BoundExpression> predicate,
        std::size_t required_count,
        parser::ast::AstNodeLocation location
    );

    [[nodiscard]] common::DatabaseId database_id() const noexcept;
    [[nodiscard]] common::CollectionId collection_id() const noexcept;
    [[nodiscard]] const std::string & collection_name() const noexcept;
    [[nodiscard]] common::VIndexId index_id() const noexcept;
    [[nodiscard]] const std::string & index_name() const noexcept;
    [[nodiscard]] common::ColumnId column_id() const noexcept;
    [[nodiscard]] const std::string & column_name() const noexcept;
    [[nodiscard]] meta::entry::VectorDistanceMetric metric() const noexcept;
    [[nodiscard]] const binder::bound::BoundExpression & query_vector() const noexcept;
    [[nodiscard]] const binder::bound::BoundExpression * predicate() const noexcept;
    [[nodiscard]] std::size_t required_count() const noexcept;

    void accept(LogicalPlanNodeVisitor & visitor) const override;
    [[nodiscard]] std::unique_ptr<LogicalPlanNode> clone() const override;

private:
    common::DatabaseId database_id_;
    common::CollectionId collection_id_;
    std::string collection_name_;
    common::VIndexId index_id_;
    std::string index_name_;
    common::ColumnId column_id_;
    std::string column_name_;
    meta::entry::VectorDistanceMetric metric_;
    std::unique_ptr<binder::bound::BoundExpression> query_vector_;
    std::unique_ptr<binder::bound::BoundExpression> predicate_;
    std::size_t required_count_;
};

} // namespace litedb::core::planner::logical

#pragma once

#include <cstddef>
#include <memory>
#include <string>

#include "core/binder/bound/expression/bound_expression.hpp"
#include "core/common/ids.hpp"
#include "core/meta/entry/vector_index_entry.hpp"
#include "core/physical_plan/node/physical_plan_node.hpp"

namespace litedb::core::physical_plan
{

class PhysicalVectorSearch final : public PhysicalPlanNode
{
public:
    PhysicalVectorSearch(
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
    [[nodiscard]] std::unique_ptr<PhysicalPlanNode> clone() const override;

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

} // namespace litedb::core::physical_plan

#pragma once

#include <memory>
#include <string>
#include <utility>
#include <vector>

#include "core/binder/bound/expression/bound_expression.hpp"
#include "core/binder/bound/statement/bound_statement.hpp"
#include "core/common/ids.hpp"
#include "core/physical_plan/statement/physical_statement_plan.hpp"

namespace litedb::core::physical_plan
{

class PhysicalInsertPlan final : public PhysicalStatementPlan
{
public:
    PhysicalInsertPlan(
        common::DatabaseId database_id,
        common::CollectionId collection_id,
        std::string collection_name,
        std::vector<binder::bound::BoundColumn> columns,
        std::vector<std::unique_ptr<binder::bound::BoundExpression>> values,
        parser::ast::AstNodeLocation location
    )
        : PhysicalStatementPlan(PhysicalStatementPlanKind::Insert, location)
        , database_id_(database_id)
        , collection_id_(collection_id)
        , collection_name_(std::move(collection_name))
        , columns_(std::move(columns))
        , values_(std::move(values))
    {
    }

    [[nodiscard]]
    common::DatabaseId database_id() const noexcept { return database_id_; }

    [[nodiscard]]
    common::CollectionId collection_id() const noexcept { return collection_id_; }

    [[nodiscard]]
    const std::string & collection_name() const noexcept { return collection_name_; }

    [[nodiscard]]
    const std::vector<binder::bound::BoundColumn> & columns() const noexcept { return columns_; }

    [[nodiscard]]
    const std::vector<std::unique_ptr<binder::bound::BoundExpression>> & values() const noexcept
    {
        return values_;
    }

private:
    common::DatabaseId database_id_;
    common::CollectionId collection_id_;
    std::string collection_name_;
    std::vector<binder::bound::BoundColumn> columns_;
    std::vector<std::unique_ptr<binder::bound::BoundExpression>> values_;
};

} // namespace litedb::core::physical_plan

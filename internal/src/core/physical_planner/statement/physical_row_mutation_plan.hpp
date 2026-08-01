#pragma once

#include <memory>
#include <string>
#include <utility>
#include <vector>

#include "core/binder/bound/statement/bound_update_statement.hpp"
#include "core/common/ids.hpp"
#include "core/physical_planner/node/physical_plan_node.hpp"
#include "core/physical_planner/statement/physical_statement_plan.hpp"

namespace litedb::core::physical_plan
{

class PhysicalDeletePlan final : public PhysicalStatementPlan
{
public:
    PhysicalDeletePlan(
        std::unique_ptr<PhysicalPlanNode> input,
        common::DatabaseId database_id,
        common::CollectionId collection_id,
        std::string collection_name,
        parser::ast::AstNodeLocation location
    );

public:
    [[nodiscard]]
    const PhysicalPlanNode & input() const noexcept;

    [[nodiscard]]
    common::DatabaseId database_id() const noexcept;

    [[nodiscard]]
    common::CollectionId collection_id() const noexcept;

    [[nodiscard]]
    const std::string & collection_name() const noexcept;

private:
    std::unique_ptr<PhysicalPlanNode> input_;
    common::DatabaseId database_id_;
    common::CollectionId collection_id_;
    std::string collection_name_;
};

class PhysicalUpdatePlan final : public PhysicalStatementPlan
{
public:
    PhysicalUpdatePlan(
        std::unique_ptr<PhysicalPlanNode> input,
        common::DatabaseId database_id,
        common::CollectionId collection_id,
        std::string collection_name,
        std::vector<binder::bound::BoundAssignment> assignments,
        parser::ast::AstNodeLocation location
    );

public:
    [[nodiscard]]
    const PhysicalPlanNode & input() const noexcept;

    [[nodiscard]]
    common::DatabaseId database_id() const noexcept;

    [[nodiscard]]
    common::CollectionId collection_id() const noexcept;

    [[nodiscard]]
    const std::string & collection_name() const noexcept;

    [[nodiscard]]
    const std::vector<binder::bound::BoundAssignment> & assignments() const noexcept;

private:
    std::unique_ptr<PhysicalPlanNode> input_;
    common::DatabaseId database_id_;
    common::CollectionId collection_id_;
    std::string collection_name_;
    std::vector<binder::bound::BoundAssignment> assignments_;
};

} // namespace litedb::core::physical_plan

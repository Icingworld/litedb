#pragma once

#include <memory>
#include <string>

#include "core/common/ids.hpp"
#include "core/physical_plan/node/physical_plan_node.hpp"
#include "core/physical_plan/statement/physical_statement_plan.hpp"

namespace litedb::core::physical_plan
{

class PhysicalRowMutationPlan final : public PhysicalStatementPlan
{
public:
    PhysicalRowMutationPlan(
        PhysicalStatementPlanKind kind,
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

} // namespace litedb::core::physical_plan

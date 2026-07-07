#pragma once

#include <memory>
#include <string>

#include "core/common/ids.hpp"
#include "core/physical_plan/node/physical_plan_node.hpp"

namespace litedb::core::physical_plan
{

class PhysicalSeqScan final : public PhysicalPlanNode
{
public:
    PhysicalSeqScan(
        common::DatabaseId database_id,
        common::CollectionId collection_id,
        std::string collection_name,
        parser::ast::AstNodeLocation location
    );

    [[nodiscard]]
    common::DatabaseId database_id() const noexcept;

    [[nodiscard]]
    common::CollectionId collection_id() const noexcept;

    [[nodiscard]]
    const std::string & collection_name() const noexcept;

    [[nodiscard]]
    std::unique_ptr<PhysicalPlanNode> clone() const override;

private:
    common::DatabaseId database_id_;
    common::CollectionId collection_id_;
    std::string collection_name_;
};

} // namespace litedb::core::physical_plan

#pragma once

#include <memory>
#include <string>
#include <vector>

#include "core/binder/bound/statement/bound_update_statement.hpp"
#include "core/common/ids.hpp"
#include "core/planner/logical/node/logical_plan_node.hpp"
#include "core/planner/plan/statement_plan.hpp"

namespace litedb::core::planner::plan
{

/**
 * @brief UPDATE ????
 */
class UpdatePlan final : public StatementPlan
{
public:
    UpdatePlan(
        std::unique_ptr<logical::LogicalPlanNode> input,
        common::DatabaseId database_id,
        common::CollectionId collection_id,
        std::string collection_name,
        std::vector<binder::bound::BoundAssignment> assignments,
        parser::ast::AstNodeLocation location
    );

public:
    /**
     * @brief ????
     * @return ??
     */
    [[nodiscard]]
    const logical::LogicalPlanNode & input() const noexcept;

    /**
     * @brief ????? ID
     * @return ??? ID
     */
    [[nodiscard]]
    common::DatabaseId database_id() const noexcept;

    /**
     * @brief ???? ID
     * @return ?? ID
     */
    [[nodiscard]]
    common::CollectionId collection_id() const noexcept;

    /**
     * @brief ??????
     * @return ????
     */
    [[nodiscard]]
    const std::string & collection_name() const noexcept;

    /**
     * @brief ????
     * @return ??
     */
    [[nodiscard]]
    const std::vector<binder::bound::BoundAssignment> & assignments() const noexcept;

private:
    std::unique_ptr<logical::LogicalPlanNode> input_;               ///< ??
    common::DatabaseId database_id_;                                ///< ??? ID
    common::CollectionId collection_id_;                            ///< ?? ID
    std::string collection_name_;                                   ///< ????
    std::vector<binder::bound::BoundAssignment> assignments_;       ///< ??
};

} // namespace litedb::core::planner::plan

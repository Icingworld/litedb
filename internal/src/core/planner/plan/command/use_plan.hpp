#pragma once

#include <string>

#include "core/common/ids.hpp"
#include "core/planner/plan/statement_plan.hpp"

namespace litedb::core::planner::plan
{

/**
 * @brief USE ????
 */
class UsePlan final : public StatementPlan
{
public:
    UsePlan(common::DatabaseId database_id, std::string database_name, parser::ast::AstNodeLocation location);

public:
    /**
     * @brief ????? ID
     * @return ??? ID
     */
    [[nodiscard]]
    common::DatabaseId database_id() const noexcept;

    /**
     * @brief ???????
     * @return ?????
     */
    [[nodiscard]]
    const std::string & database_name() const noexcept;

private:
    common::DatabaseId database_id_;             ///< ??? ID
    std::string database_name_;                  ///< ?????
};

} // namespace litedb::core::planner::plan

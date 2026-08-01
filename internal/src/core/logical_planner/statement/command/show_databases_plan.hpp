#pragma once

#include "core/logical_planner/statement/logical_statement_plan.hpp"

namespace litedb::core::planner::plan
{

/**
 * @brief SHOW DATABASES 语句计划
 */
class ShowDatabasesPlan final : public LogicalStatementPlan
{
public:
    explicit ShowDatabasesPlan(parser::ast::AstNodeLocation location);
};

} // namespace litedb::core::planner::plan

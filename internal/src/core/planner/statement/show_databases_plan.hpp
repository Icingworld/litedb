#pragma once

#include "core/planner/statement/statement_plan.hpp"

namespace litedb::core::planner
{

/**
 * @brief SHOW DATABASES 语句计划
 */
class ShowDatabasesPlan final : public StatementPlan
{
public:
    explicit ShowDatabasesPlan(parser::ast::AstNodeLocation location);
};

} // namespace litedb::core::planner

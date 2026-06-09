#include "core/planner/statement/create_database_plan.hpp"

#include <utility>

namespace litedb::core::planner
{

CreateDatabasePlan::CreateDatabasePlan(
    std::string database_name,
    bool if_not_exists,
    parser::ast::AstNodeLocation location
)
    : StatementPlan(StatementPlanKind::CreateDatabase, location)
    , database_name_(std::move(database_name))
    , if_not_exists_(if_not_exists)
{
}

const std::string & CreateDatabasePlan::database_name() const noexcept
{
    return database_name_;
}

bool CreateDatabasePlan::if_not_exists() const noexcept
{
    return if_not_exists_;
}
} // namespace litedb::core::planner

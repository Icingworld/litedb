#include "core/logical_planner/logical_planner_helper.hpp"

namespace litedb::core::logical_planner
{

[[nodiscard]]
LogicalPlannerError make_planner_error(
    LogicalPlannerErrorCode code,
    parser::ast::AstNodeLocation location,
    std::string message
)
{
    return LogicalPlannerError {
        code,
        message,
        LogicalPlannerErrorContext {
            location
        }
    };
}

[[nodiscard]]
LogicalPlannerError make_planner_error(
    LogicalPlannerErrorCode code,
    std::string message
)
{
    return LogicalPlannerError {
        code,
        message,
    };
}

} // namespace litedb::core::logical_planner
#include "core/logical_planner/logical_planner_helper.hpp"

namespace litedb::core::logical_planner
{

[[nodiscard]]
PlannerError make_planner_error(
    PlannerErrorCode code,
    parser::ast::AstNodeLocation location,
    std::string message
)
{
    return PlannerError {
        code,
        message,
        PlannerErrorContext {
            location
        }
    };
}

[[nodiscard]]
PlannerError make_planner_error(
    PlannerErrorCode code,
    std::string message
)
{
    return PlannerError {
        code,
        message,
    };
}

} // namespace litedb::core::logical_planner
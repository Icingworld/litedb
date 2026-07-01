#include "core/planner/planner_helper.hpp"

namespace litedb::core::planner
{

[[nodiscard]]
PlannerError make_planner_error(
    PlannerErrorCode code,
    parser::ast::AstNodeLocation location,
    std::string message
)
{
    return PlannerError {code, location, message};
}

} // namespace litedb::core::planner
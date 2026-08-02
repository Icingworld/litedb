#pragma once

#include "core/logical_planner/logical_planner_error.hpp"

namespace litedb::core::logical_planner
{

/**
 * @brief 创建计划器错误
 * @param code 错误码
 * @param location 错误位置
 * @param message 错误消息
 * @return 计划器错误
 */
[[nodiscard]]
PlannerError make_planner_error(
    PlannerErrorCode code,
    parser::ast::AstNodeLocation location,
    std::string message
);

/**
 * @brief 创建计划器错误
 * @param code 错误码
 * @param message 错误消息
 * @return 计划器错误
 */
[[nodiscard]]
PlannerError make_planner_error(
    PlannerErrorCode code,
    std::string message
);

} // namespace litedb::core::logical_planner

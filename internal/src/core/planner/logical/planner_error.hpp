#pragma once

#include <string>

#include "core/parser/ast/ast_node.hpp"

namespace litedb::core::planner::logical
{

/**
 * @brief 计划错误码
 */
enum class PlannerErrorCode
{
    InvalidArgument,                ///< 无效参数
    UnsupportedStatement            ///< 不支持的语句
};

/**
 * @brief 计划错误
 */
struct PlannerError
{
    PlannerErrorCode code;                      ///< 错误码
    parser::ast::AstNodeLocation location;      ///< 错误位置
    std::string message;                        ///< 错误消息
};

} // namespace litedb::core::planner::logical

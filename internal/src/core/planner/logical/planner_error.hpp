#pragma once

#include <cstdint>
#include <string>

#include "core/parser/ast/ast_node.hpp"

namespace litedb::core::planner::logical
{

enum class PlannerErrorCode : std::uint8_t
{
    InvalidArgument,
    UnsupportedStatement,
};

struct PlannerError
{
    PlannerErrorCode code;
    parser::ast::AstNodeLocation location;
    std::string message;
};

} // namespace litedb::core::planner::logical

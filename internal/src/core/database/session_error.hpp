#pragma once

#include <cstdint>
#include <optional>

#include "core/error/error.hpp"
#include "core/parser/ast/ast_node.hpp"

namespace litedb::core::database
{

enum class SessionErrorCode : std::uint8_t
{
    ParserError = 0,
    BinderError = 1,
    ExecutionError = 4,
};

struct SessionErrorContext
{
    std::optional<parser::ast::AstNodeLocation> location;
};

using SessionError = error::Error;

} // namespace litedb::core::database

namespace litedb::core::error
{
template <>
struct ErrorTraits<database::SessionErrorCode>
{
    static constexpr ErrorCategory category = ErrorCategory::Database;
};
} // namespace litedb::core::error

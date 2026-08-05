#include "core/database/session.hpp"

#include <memory>
#include <mutex>
#include <string>
#include <utility>

#include "core/binder/binder.hpp"
#include "core/function/builtin/builtin_functions.hpp"
#include "core/executor/executor.hpp"
#include "core/optimizer/optimizer.hpp"
#include "core/parser/ast/statement/statement_node.hpp"
#include "core/parser/parser.hpp"
#include "core/physical_planner/physical_planner.hpp"
#include "core/logical_planner/logical_planner.hpp"

namespace litedb::core::database
{

namespace
{

[[nodiscard]]
parser::ast::AstNodeLocation location_from_token(parser::TokenLocation location) noexcept
{
    return parser::ast::AstNodeLocation {
        .line = location.line,
        .column = location.column,
    };
}

[[nodiscard]]
SessionError from_parser_error(parser::ParserError error)
{
    const auto * context = error.context<parser::ParserErrorContext>();
    const auto location = context == nullptr
        ? std::optional<parser::ast::AstNodeLocation> {}
        : std::optional<parser::ast::AstNodeLocation> {location_from_token(context->location)};
    auto message = error.message();
    return SessionError {
        SessionErrorCode::ParserError,
        message,
        SessionErrorContext {location},
        std::move(error),
    };
}

[[nodiscard]]
SessionError from_binder_error(binder::BinderError error)
{
    const auto * context = error.context<binder::BinderErrorContext>();
    const auto location = context == nullptr
        ? std::optional<parser::ast::AstNodeLocation> {}
        : std::optional<parser::ast::AstNodeLocation> {context->location};
    auto message = error.message();
    return SessionError {SessionErrorCode::BinderError, message, SessionErrorContext {location}, std::move(error)};
}

[[nodiscard]]
SessionError from_execution_error(executor::ExecutionError error)
{
    auto message = error.message();
    return SessionError {
        SessionErrorCode::ExecutionError,
        message,
        SessionErrorContext {std::nullopt},
        std::move(error)
    };
}

} // namespace

Session::Session(DatabaseEngine & engine) noexcept
    : engine_(&engine)
{
}

std::expected<executor::ExecutionResult, SessionError> Session::execute_sql(std::string_view sql)
{
    std::scoped_lock lock {engine_->mutex_};

    parser::Parser parser {std::string(sql)};
    auto parsed = parser.parse();
    if (!parsed.has_value()) {
        return std::unexpected(from_parser_error(std::move(parsed.error())));
    }

    binder::BinderContext context {engine_->meta(), session_, function::builtin::builtin_function_catalog()};
    binder::Binder binder {context};
    auto bound = binder.bind(**parsed);
    if (!bound.has_value()) {
        return std::unexpected(from_binder_error(std::move(bound.error())));
    }

    logical_planner::LogicalPlanner planner;
    auto planned = planner.plan(std::move(*bound));

    optimizer::Optimizer optimizer {};
    auto optimized = optimizer.optimize(std::move(planned));

    physical_planner::PhysicalPlanner physical_planner {engine_->meta()};
    auto physical = physical_planner.plan(std::move(optimized));

    auto executed = engine_->execute(*physical);
    if (!executed.has_value()) {
        return std::unexpected(from_execution_error(std::move(executed.error())));
    }

    if (executed->kind == executor::ExecutionResultKind::UseDatabase) {
        session_.current_database_id = executed->selected_database_id;
    }

    return std::move(*executed);
}

std::optional<common::DatabaseId> Session::current_database_id() const noexcept
{
    return session_.current_database_id;
}

} // namespace litedb::core::database

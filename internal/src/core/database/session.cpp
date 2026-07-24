#include "core/database/session.hpp"

#include <memory>
#include <mutex>
#include <string>
#include <utility>

#include "core/binder/binder.hpp"
#include "core/executor/executor.hpp"
#include "core/optimizer/optimizer.hpp"
#include "core/parser/ast/statement/statement_node.hpp"
#include "core/parser/parser.hpp"
#include "core/physical_plan/physical_planner.hpp"
#include "core/logical_plan/logical_planner.hpp"

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
    return SessionError {
        .code = SessionErrorCode::ParserError,
        .location = location_from_token(error.location),
        .message = std::move(error.message),
    };
}

[[nodiscard]]
SessionError from_binder_error(binder::BinderError error)
{
    return SessionError {
        .code = SessionErrorCode::BinderError,
        .location = error.location,
        .message = std::move(error.message),
    };
}

[[nodiscard]]
SessionError from_planner_error(planner::PlannerError error)
{
    return SessionError {
        .code = SessionErrorCode::PlannerError,
        .location = error.location,
        .message = std::move(error.message),
    };
}

[[nodiscard]]
SessionError from_optimizer_error(optimizer::OptimizerError error)
{
    return SessionError {
        .code = SessionErrorCode::OptimizerError,
        .location = error.location,
        .message = std::move(error.message),
    };
}

[[nodiscard]]
SessionError from_execution_error(executor::ExecutionError error)
{
    return SessionError {
        .code = SessionErrorCode::ExecutionError,
        .location = error.location,
        .message = std::move(error.message),
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

    binder::BinderContext context {engine_->meta(), session_};
    binder::Binder binder {context};
    auto bound = binder.bind(*parsed.value());
    if (!bound.has_value()) {
        return std::unexpected(from_binder_error(std::move(bound.error())));
    }

    planner::logical::LogicalPlanner planner;
    auto planned = planner.plan(std::move(bound.value()));
    if (!planned.has_value()) {
        return std::unexpected(from_planner_error(std::move(planned.error())));
    }

    optimizer::Optimizer optimizer {{}, engine_->meta()};
    auto optimized = optimizer.optimize(std::move(planned.value()));
    if (!optimized.has_value()) {
        return std::unexpected(from_optimizer_error(std::move(optimized.error())));
    }

    physical_plan::PhysicalPlanner physical_planner;
    auto physical = physical_planner.plan(*optimized.value());

    auto executed = engine_->execute(*physical);
    if (!executed.has_value()) {
        return std::unexpected(from_execution_error(std::move(executed.error())));
    }

    if (executed->kind == executor::ExecutionResultKind::UseDatabase) {
        session_.current_database_id = executed->selected_database_id;
    }

    return std::move(executed.value());
}

std::optional<common::DatabaseId> Session::current_database_id() const noexcept
{
    return session_.current_database_id;
}

} // namespace litedb::core::database

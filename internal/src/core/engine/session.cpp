#include "core/engine/session.hpp"

#include <memory>
#include <mutex>
#include <string>
#include <utility>

#include "core/binder/binder.hpp"
#include "core/executor/executor.hpp"
#include "core/parser/ast/statement/statement_node.hpp"
#include "core/parser/parser.hpp"
#include "core/planner/planner.hpp"

namespace litedb::core::engine
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
EngineError from_parser_error(parser::ParserError error)
{
    return EngineError {
        .code = EngineErrorCode::ParserError,
        .location = location_from_token(error.location),
        .message = std::move(error.message),
    };
}

[[nodiscard]]
EngineError from_binder_error(binder::BinderError error)
{
    return EngineError {
        .code = EngineErrorCode::BinderError,
        .location = error.location,
        .message = std::move(error.message),
    };
}

[[nodiscard]]
EngineError from_planner_error(planner::logical::PlannerError error)
{
    return EngineError {
        .code = EngineErrorCode::PlannerError,
        .location = error.location,
        .message = std::move(error.message),
    };
}

[[nodiscard]]
EngineError from_execution_error(executor::ExecutionError error)
{
    return EngineError {
        .code = EngineErrorCode::ExecutionError,
        .location = error.location,
        .message = std::move(error.message),
    };
}

} // namespace

Session::Session(DatabaseInstance & instance) noexcept
    : instance_(&instance)
{
}

std::expected<executor::ExecutionResult, EngineError> Session::execute_sql(std::string_view sql)
{
    std::scoped_lock lock {instance_->mutex()};

    parser::Parser parser {std::string(sql)};
    auto parsed = parser.parse();
    if (!parsed.has_value()) {
        return std::unexpected(from_parser_error(std::move(parsed.error())));
    }

    binder::Binder binder {instance_->catalog(), session_};
    auto bound = binder.bind(*parsed.value());
    if (!bound.has_value()) {
        return std::unexpected(from_binder_error(std::move(bound.error())));
    }

    planner::Planner planner;
    auto planned = planner.plan(std::move(bound.value()));
    if (!planned.has_value()) {
        return std::unexpected(from_planner_error(std::move(planned.error())));
    }

    executor::Executor executor {instance_->catalog(), instance_->storage(), instance_->ddl_handler()};
    auto executed = executor.execute(*planned.value());
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

} // namespace litedb::core::engine

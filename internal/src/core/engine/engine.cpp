#include "core/engine/engine.hpp"

#include <memory>
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

std::expected<executor::ExecutionResult, EngineError> Engine::execute_sql(std::string_view sql)
{
    // 解析 SQL
    parser::Parser parser {std::string(sql)};
    auto parsed = parser.parse();
    if (!parsed.has_value()) {
        return std::unexpected(from_parser_error(std::move(parsed.error())));
    }

    // 绑定 AST 树
    binder::Binder binder {catalog_, session_};
    auto bound = binder.bind(*parsed.value());
    if (!bound.has_value()) {
        return std::unexpected(from_binder_error(std::move(bound.error())));
    }

    // 构建逻辑计划
    planner::Planner planner;
    auto planned = planner.plan(std::move(bound.value()));
    if (!planned.has_value()) {
        return std::unexpected(from_planner_error(std::move(planned.error())));
    }

    // 执行逻辑计划
    executor::Executor executor {catalog_, storage_};
    auto executed = executor.execute(*planned.value());
    if (!executed.has_value()) {
        return std::unexpected(from_execution_error(std::move(executed.error())));
    }

    // 针对 USE 语句，更新会话上下文
    if (executed->kind == executor::ExecutionResultKind::UseDatabase) {
        session_.current_database_id = executed->selected_database_id;
    }

    return std::move(executed.value());
}

std::optional<common::DatabaseId> Engine::current_database_id() const noexcept
{
    return session_.current_database_id;
}

catalog::InMemoryCatalog & Engine::catalog() noexcept
{
    return catalog_;
}

const catalog::InMemoryCatalog & Engine::catalog() const noexcept
{
    return catalog_;
}

storage::StorageManager & Engine::storage() noexcept
{
    return storage_;
}

const storage::StorageManager & Engine::storage() const noexcept
{
    return storage_;
}

} // namespace litedb::core::engine

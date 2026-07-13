#include "core/binder/worker/binder_update_worker.hpp"

#include "core/binder/binder_helper.hpp"
#include "core/binder/binder_context.hpp"
#include <unordered_set>
#include "core/binder/bound/statement/bound_update_statement.hpp"
#include "core/meta/meta.hpp"
#include "core/parser/ast/statement/update_statement.hpp"
#include "core/binder/worker/binder_worker_helper.hpp"

namespace litedb::core::binder
{

using namespace litedb::core::binder::bound;
using namespace litedb::core::common;
using namespace litedb::core::parser;
using namespace litedb::core::parser::ast;

BinderUpdateWorker::BinderUpdateWorker(const BinderContext & context) noexcept
    : context_(context)
{
}

std::expected<std::unique_ptr<BoundStatement>, BinderError> BinderUpdateWorker::bind_update(
    const UpdateStatement & statement
)
{
    BinderWorkerHelper helper(context_);

    auto collection = helper.bind_collection(statement.collection(), statement.location());
    if (!collection.has_value()) [[unlikely]] {
        return std::unexpected(std::move(collection.error()));
    }

    std::vector<BoundAssignment> assignments;
    std::unordered_set<std::string> seen_columns;
    for (const auto & assignment : statement.assignments()) {
        const auto column_key = meta::normalize_identifier(assignment.column);
        if (!seen_columns.emplace(column_key).second) [[unlikely]] {
            return std::unexpected(make_binder_error(
                BinderErrorCode::DuplicateColumn,
                statement.location(),
                "Duplicate UPDATE target column: " + assignment.column
            ));
        }

        const auto * column = context_.meta().find_column(collection->collection->id(), assignment.column);
        if (column == nullptr) [[unlikely]] {
            return std::unexpected(make_binder_error(
                BinderErrorCode::ColumnNotFound,
                statement.location(),
                "Column not found: " + assignment.column
            ));
        }

        auto value = helper.bind_expression(*assignment.value, collection.value());
        if (!value.has_value()) [[unlikely]] {
            return std::unexpected(std::move(value.error()));
        }
        if (!can_cast(value.value()->type(), column->type())) [[unlikely]] {
            return std::unexpected(make_binder_error(
                BinderErrorCode::InvalidType,
                value.value()->location(),
                "UPDATE value type does not match column: " + column->name()
            ));
        }
        if (value.value()->type().id == LogicalTypeId::Null && !column->nullable()) [[unlikely]] {
            return std::unexpected(make_binder_error(
                BinderErrorCode::NotNullable,
                value.value()->location(),
                "Column cannot be NULL: " + column->name()
            ));
        }

        assignments.push_back(BoundAssignment {
            .column = bound_column_from_entry(*column),
            .value = cast_if_needed(std::move(value.value()), column->type()),
        });
    }

    // 绑定条件表达式
    std::unique_ptr<BoundExpression> where;
    if (statement.where() != nullptr) {
        auto bound_where = helper.bind_expression(*statement.where(), collection.value());
        if (!bound_where.has_value()) [[unlikely]] {
            return std::unexpected(std::move(bound_where.error()));
        }
        if (!is_boolean(bound_where.value()->type())) [[unlikely]] {
            return std::unexpected(make_binder_error(
                BinderErrorCode::InvalidType,
                statement.where()->location(),
                "WHERE expression must be BOOLEAN"
            ));
        }
        where = std::move(bound_where.value());
    }

    return std::make_unique<BoundUpdateStatement>(
        collection->database_id,
        collection->collection->id(),
        collection->collection->name(),
        std::move(assignments),
        std::move(where),
        statement.location()
    );
}

} // namespace litedb::core::binder

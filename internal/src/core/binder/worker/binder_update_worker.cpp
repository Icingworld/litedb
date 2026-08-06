#include "core/binder/worker/binder_update_worker.hpp"

#include "core/binder/binder_helper.hpp"
#include "core/binder/binder_context.hpp"
#include <unordered_set>
#include "core/binder/bound/statement/bound_update_statement.hpp"
#include "core/parser/ast/statement/update_statement.hpp"
#include "core/binder/worker/binder_worker_helper.hpp"
#include "core/common/identifier.hpp"

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

std::expected<std::unique_ptr<BoundStatement>, BinderError>
BinderUpdateWorker::bind_update(
    const UpdateStatement & statement
)
{
    BinderWorkerHelper helper(context_);

    // 通过 Helper 绑定集合
    auto collection = helper.bind_collection(
        statement.collection_name()
    );
    if (!collection.has_value()) [[unlikely]] {
        return std::unexpected(std::move(collection.error()));
    }

    // 绑定赋值列表
    std::vector<BoundAssignment> assignments;
    std::unordered_set<std::string> seen_columns;
    for (const auto & assignment : statement.assignments()) {
        const auto column_key = normalize_identifier(assignment.column_name);
        if (!seen_columns.emplace(column_key).second) [[unlikely]] {
            return std::unexpected(make_binder_error(
                BinderErrorCode::DuplicateColumn,
                "Duplicate UPDATE target column: " + assignment.column_name
            ));
        }

        // 查找列
        const auto * column = context_.meta().find_column(
            collection->collection->id(),
            assignment.column_name
        );
        if (column == nullptr) [[unlikely]] {
            return std::unexpected(make_binder_error(
                BinderErrorCode::ColumnNotFound,
                "Column not found: " + assignment.column_name
            ));
        }

        // 通过 Helper 绑定值
        auto value = helper.bind_expression(*assignment.value, *collection);
        if (!value.has_value()) [[unlikely]] {
            return std::unexpected(std::move(value.error()));
        }
        if (!can_cast((*value)->type(), column->type())) [[unlikely]] {
            return std::unexpected(make_binder_error(
                BinderErrorCode::InvalidType,
                "UPDATE value type does not match column: " + column->name()
            ));
        }
        if ((*value)->type().id == LogicalTypeId::Null && !column->nullable()) [[unlikely]] {
            return std::unexpected(make_binder_error(
                BinderErrorCode::NotNullable,
                "Column cannot be NULL: " + column->name()
            ));
        }

        assignments.push_back(BoundAssignment {
            .column_id = column->id(),
            .value = cast_if_needed(std::move(*value), column->type()),
        });
    }

    // 通过 Helper 绑定条件表达式
    std::unique_ptr<BoundExpression> where;
    if (statement.where() != nullptr) {
        auto bound_where = helper.bind_expression(
            *statement.where(),
            *collection
        );
        if (!bound_where.has_value()) [[unlikely]] {
            return std::unexpected(std::move(bound_where.error()));
        }
        if (!is_boolean((*bound_where)->type())) [[unlikely]] {
            return std::unexpected(make_binder_error(
                BinderErrorCode::InvalidType,
                "WHERE expression must be BOOLEAN"
            ));
        }
        where = std::move(*bound_where);
    }

    return std::make_unique<BoundUpdateStatement>(
        collection->collection->id(),
        std::move(assignments),
        std::move(where)
    );
}

} // namespace litedb::core::binder

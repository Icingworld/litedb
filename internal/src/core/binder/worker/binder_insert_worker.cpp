#include "core/binder/worker/binder_insert_worker.hpp"

#include "core/binder/binder_helper.hpp"
#include "core/binder/binder_context.hpp"
#include <algorithm>
#include <optional>
#include <unordered_set>
#include "core/binder/bound/expression/bound_null_expression.hpp"
#include "core/binder/bound/statement/bound_insert_statement.hpp"
#include "core/meta/meta.hpp"
#include "core/parser/ast/statement/insert_statement.hpp"
#include "core/binder/worker/binder_worker_helper.hpp"

namespace litedb::core::binder
{

using namespace litedb::core::binder::bound;
using namespace litedb::core::common;
using namespace litedb::core::parser;
using namespace litedb::core::parser::ast;

BinderInsertWorker::BinderInsertWorker(const BinderContext & context) noexcept
    : context_(context)
{
}

std::expected<std::unique_ptr<BoundStatement>, BinderError> BinderInsertWorker::bind_insert(
    const InsertStatement & statement
)
{
    BinderWorkerHelper helper(context_);

    auto collection = helper.bind_collection(
        statement.collection_name(),
        statement.location()
    );
    if (!collection.has_value()) [[unlikely]] {
        return std::unexpected(std::move(collection.error()));
    }

    const auto catalog_columns = context_.meta().list_columns(collection->collection->id());
    std::vector<const meta::entry::ColumnEntry *> target_columns;
    target_columns.reserve(catalog_columns.size());
    std::vector<std::optional<std::size_t>> source_value_by_target;

    if (statement.columns().empty()) {
        // 没有指定列，使用所有列
        if (statement.values().size() != catalog_columns.size()) {
            return std::unexpected(make_binder_error(
                BinderErrorCode::InvalidValueCount,
                statement.location(),
                "INSERT value count does not match collection column count"
            ));
        }
        for (std::size_t index = 0; index < catalog_columns.size(); ++index) {
            target_columns.push_back(catalog_columns[index]);
            source_value_by_target.emplace_back(index);
        }
    } else {
        // 指定了列，检查列和值的数量是否匹配
        if (statement.columns().size() != statement.values().size()) {
            return std::unexpected(make_binder_error(
                BinderErrorCode::InvalidValueCount,
                statement.location(),
                "INSERT column count does not match value count"
            ));
        }

        // 检查列是否重复
        std::unordered_set<std::string> seen_columns;
        for (std::size_t index = 0; index < statement.columns().size(); ++index) {
            const auto column_key = common::normalize_identifier(statement.columns()[index]);
            if (!seen_columns.emplace(column_key).second) {
                return std::unexpected(make_binder_error(
                    BinderErrorCode::DuplicateColumn,
                    statement.location(),
                    "Duplicate INSERT target column: " + statement.columns()[index]
                ));
            }
        }

        // 使用所有列
        for (const auto * column : catalog_columns) {
            target_columns.push_back(column);
            source_value_by_target.emplace_back(std::nullopt);
        }

        // 遍历指定列，绑定列引用
        for (std::size_t index = 0; index < statement.columns().size(); ++index) {
            const auto * column = context_.meta().find_column(collection->collection->id(), statement.columns()[index]);
            if (column == nullptr) [[unlikely]] {
                return std::unexpected(make_binder_error(
                    BinderErrorCode::ColumnNotFound,
                    statement.location(),
                    "Column not found: " + statement.columns()[index]
                ));
            }

            const auto target_it = std::ranges::find(target_columns, column);
            source_value_by_target[static_cast<std::size_t>(target_it - target_columns.begin())] = index;
        }
    }

    // 绑定列和值
    std::vector<BoundColumn> bound_columns;
    std::vector<std::unique_ptr<BoundExpression>> bound_values;
    bound_columns.reserve(target_columns.size());
    bound_values.reserve(target_columns.size());

    for (std::size_t target_index = 0; target_index < target_columns.size(); ++target_index) {
        const auto & column = *target_columns[target_index];
        bound_columns.push_back(bound_column_from_entry(column));

        std::unique_ptr<BoundExpression> value;
        if (source_value_by_target[target_index].has_value()) {
            auto expression = helper.bind_expression(
                *statement.values()[source_value_by_target[target_index].value()],
                *collection
            );
            if (!expression.has_value()) [[unlikely]] {
                return std::unexpected(std::move(expression.error()));
            }
            value = std::move(*expression);
        } else if (column.default_expression().has_value()) {
            auto expression = helper.bind_default_expression(column.default_expression().value(), statement.location());
            if (!expression.has_value()) [[unlikely]] {
                return std::unexpected(std::move(expression.error()));
            }
            value = std::move(*expression);
        } else if (column.nullable()) [[likely]] {
            value = std::make_unique<BoundNullExpression>(column.type(), statement.location());
        } else [[unlikely]] {
            return std::unexpected(make_binder_error(
                BinderErrorCode::NotNullable,
                statement.location(),
                "Column requires a value: " + column.name()
            ));
        }

        // 检查值类型是否匹配列类型
        if (!can_cast(value->type(), column.type())) [[unlikely]] {
            return std::unexpected(make_binder_error(
                BinderErrorCode::InvalidType,
                value->location(),
                "INSERT value type " + type_name(value->type())
                    + " does not match column " + column.name()
                    + " type " + type_name(column.type())
            ));
        }
        // 检查值是否为 NULL
        if (value->type().id == LogicalTypeId::Null && !column.nullable()) [[unlikely]] {
            return std::unexpected(make_binder_error(
                BinderErrorCode::NotNullable,
                value->location(),
                "Column cannot be NULL: " + column.name()
            ));
        }

        bound_values.push_back(cast_if_needed(std::move(value), column.type()));
    }

    return std::make_unique<BoundInsertStatement>(
        collection->database_id,
        collection->collection->id(),
        collection->collection->name(),
        std::move(bound_columns),
        std::move(bound_values),
        statement.location()
    );
}

} // namespace litedb::core::binder

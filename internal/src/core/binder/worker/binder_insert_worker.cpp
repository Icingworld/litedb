#include "core/binder/worker/binder_insert_worker.hpp"

#include <algorithm>
#include <optional>
#include <unordered_map>
#include <unordered_set>

#include "core/binder/bound/expression/bound_null_expression.hpp"
#include "core/binder/bound/statement/bound_insert_statement.hpp"
#include "core/binder/worker/binder_worker_helper.hpp"
#include "core/binder/binder_helper.hpp"
#include "core/binder/binder_context.hpp"
#include "core/common/identifier.hpp"
#include "core/parser/ast/dispatcher/expression_dispatcher.hpp"
#include "core/parser/ast/statement/insert_statement.hpp"


namespace litedb::core::binder
{

using namespace litedb::core::binder::bound;
using namespace litedb::core::common;
using namespace litedb::core::parser;
using namespace litedb::core::parser::ast;

namespace
{

/**
 * @brief 插入值表达式验证器
 */
class InsertValueExpressionValidator final
    : private AstExpressionDispatcher<InsertValueExpressionValidator, bool>
{
    friend class AstExpressionDispatcher<InsertValueExpressionValidator, bool>;

public:
    /**
     * @brief 验证插入值表达式是否合法
     * @param expression 表达式
     * @return 是否合法
     */
    [[nodiscard]]
    bool validate(const ExpressionNode & expression)
    {
        return dispatch_expression(expression);
    }

private:
    /**
     * @brief 访问标识符表达式
     * @param expression 表达式
     * @return 是否合法
     */
    [[nodiscard]] bool visit_identifier_expression(const IdentifierExpression &) const noexcept
    {
        return false;
    }

    /**
     * @brief 访问通配符表达式
     * @param expression 表达式
     * @return 是否合法
     */
    [[nodiscard]]
    bool visit_wildcard_expression(const WildcardExpression &) const noexcept
    {
        return false;
    }

    /**
     * @brief 访问字面量表达式
     * @param expression 表达式
     * @return 是否合法
     */
    [[nodiscard]]
    bool visit_literal_expression(const LiteralExpression &) const noexcept
    {
        return true;
    }

    /**
     * @brief 访问列引用表达式
     * @param expression 表达式
     * @return 是否合法
     */
    [[nodiscard]]
    bool visit_column_reference_expression(const ColumnReferenceExpression &) const noexcept
    {
        return false;
    }

    /**
     * @brief 访问函数调用表达式
     * @param expression 表达式
     * @return 是否合法
     */
    [[nodiscard]]
    bool visit_function_call_expression(const FunctionCallExpression & expression)
    {
        return std::ranges::all_of(expression.arguments(), [this](const auto & argument) {
            return validate(*argument);
        });
    }

    /**
     * @brief 访问向量表达式
     * @param expression 表达式
     * @return 是否合法
     */
    [[nodiscard]]
    bool visit_vector_expression(const VectorExpression & expression)
    {
        return std::ranges::all_of(expression.elements(), [this](const auto & element) {
            return validate(*element);
        });
    }

    /**
     * @brief 访问二元表达式
     * @param expression 表达式
     * @return 是否合法
     */
    [[nodiscard]]
    bool visit_binary_expression(const BinaryExpression & expression)
    {
        return validate(expression.left()) && validate(expression.right());
    }

    /**
     * @brief 访问一元表达式
     * @param expression 表达式
     * @return 是否合法
     */
    [[nodiscard]]
    bool visit_unary_expression(const UnaryExpression & expression)
    {
        return validate(expression.operand());
    }

    /**
     * @brief 访问 IN 表达式
     * @param expression 表达式
     * @return 是否合法
     */
    [[nodiscard]]
    bool visit_in_expression(const InExpression & expression)
    {
        return validate(expression.expression())
            && std::ranges::all_of(expression.values(), [this](const auto & value) {
                return validate(*value);
            });
    }

    /**
     * @brief 访问 BETWEEN 表达式
     * @param expression 表达式
     * @return 是否合法
     */
    [[nodiscard]]
    bool visit_between_expression(const BetweenExpression & expression)
    {
        return validate(expression.expression())
            && validate(expression.lower())
            && validate(expression.upper());
    }

    /**
     * @brief 访问 LIKE 表达式
     * @param expression 表达式
     * @return 是否合法
     */
    [[nodiscard]]
    bool visit_like_expression(const LikeExpression & expression)
    {
        return validate(expression.expression()) && validate(expression.pattern());
    }

    /**
     * @brief 访问别名表达式
     * @param expression 表达式
     * @return 是否合法
     */
    [[nodiscard]]
    bool visit_alias_expression(const AliasExpression & expression)
    {
        return validate(expression.expression());
    }
};

} // namespace

BinderInsertWorker::BinderInsertWorker(const BinderContext & context) noexcept
    : context_(context)
{
}

std::expected<std::unique_ptr<BoundStatement>, BinderError>
BinderInsertWorker::bind_insert(
    const InsertStatement & statement
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

    // 获取集合所有列
    const auto catalog_columns = context_.meta().list_columns(
        collection->collection->id()
    );
    std::vector<std::optional<std::size_t>> source_value_by_target(
        catalog_columns.size()
    );
    std::unordered_map<ColumnId, std::size_t> target_index_by_column_id;
    target_index_by_column_id.reserve(catalog_columns.size());
    for (std::size_t index = 0; index < catalog_columns.size(); ++index) {
        target_index_by_column_id.emplace(catalog_columns[index]->id(), index);
    }

    if (statement.columns().empty()) {
        // 没有指定列，使用所有列
        if (statement.values().size() != catalog_columns.size()) [[unlikely]] {
            return std::unexpected(make_binder_error(
                BinderErrorCode::InvalidValueCount,
                "INSERT value count does not match collection column count"
            ));
        }
        for (std::size_t index = 0; index < catalog_columns.size(); ++index) {
            source_value_by_target[index] = index;
        }
    } else {
        // 指定了列，检查列和值的数量是否匹配
        if (statement.columns().size() != statement.values().size()) {
            return std::unexpected(make_binder_error(
                BinderErrorCode::InvalidValueCount,
                "INSERT column count does not match value count"
            ));
        }

        // 检查列是否重复
        std::unordered_set<std::string> seen_columns;
        for (std::size_t index = 0; index < statement.columns().size(); ++index) {
            const auto column_key = normalize_identifier(statement.columns()[index]);
            if (!seen_columns.emplace(column_key).second) {
                return std::unexpected(make_binder_error(
                    BinderErrorCode::DuplicateColumn,
                    "Duplicate INSERT target column: " + statement.columns()[index]
                ));
            }
        }

        // 遍历指定列，绑定列引用
        for (std::size_t index = 0; index < statement.columns().size(); ++index) {
            const auto * column = context_.meta().find_column(collection->collection->id(), statement.columns()[index]);
            if (column == nullptr) [[unlikely]] {
                return std::unexpected(make_binder_error(
                    BinderErrorCode::ColumnNotFound,
                    "Column not found: " + statement.columns()[index]
                ));
            }

            const auto target_it = target_index_by_column_id.find(column->id());
            if (target_it == target_index_by_column_id.end()) [[unlikely]] {
                return std::unexpected(make_binder_error(
                    BinderErrorCode::ColumnNotFound,
                    "Column is not part of the target collection: " + column->name()
                ));
            }
            source_value_by_target[target_it->second] = index;
        }
    }

    // 按集合完整列顺序绑定和展开值
    std::vector<std::unique_ptr<BoundExpression>> bound_values;
    bound_values.reserve(catalog_columns.size());

    for (std::size_t target_index = 0; target_index < catalog_columns.size(); ++target_index) {
        const auto & column = *catalog_columns[target_index];

        std::unique_ptr<BoundExpression> value;
        if (source_value_by_target[target_index].has_value()) {
            const auto & source_expression =
                *statement.values()[*source_value_by_target[target_index]];
            if (!InsertValueExpressionValidator {}.validate(source_expression)) [[unlikely]] {
                return std::unexpected(make_binder_error(
                    BinderErrorCode::UnsupportedExpression,
                    "INSERT VALUES expressions cannot reference collection columns"
                ));
            }
            auto expression = helper.bind_expression(
                source_expression,
                *collection
            );
            if (!expression.has_value()) [[unlikely]] {
                return std::unexpected(std::move(expression.error()));
            }
            value = std::move(*expression);
        } else if (column.default_expression().has_value()) {
            auto expression = helper.bind_default_expression(column.default_expression().value());
            if (!expression.has_value()) [[unlikely]] {
                return std::unexpected(std::move(expression.error()));
            }
            value = std::move(*expression);
        } else if (column.nullable()) [[likely]] {
            value = std::make_unique<BoundNullExpression>(column.type());
        } else [[unlikely]] {
            return std::unexpected(make_binder_error(
                BinderErrorCode::NotNullable,
                "Column requires a value: " + column.name()
            ));
        }

        // 检查值类型是否匹配列类型
        if (!can_cast(value->type(), column.type())) [[unlikely]] {
            return std::unexpected(make_binder_error(
                BinderErrorCode::InvalidType,
                "INSERT value type " + type_name(value->type())
                    + " does not match column " + column.name()
                    + " type " + type_name(column.type())
            ));
        }
        // 检查值是否为 NULL
        if (value->type().id == LogicalTypeId::Null && !column.nullable()) [[unlikely]] {
            return std::unexpected(make_binder_error(
                BinderErrorCode::NotNullable,
                "Column cannot be NULL: " + column.name()
            ));
        }

        bound_values.push_back(cast_if_needed(std::move(value), column.type()));
    }

    return std::make_unique<BoundInsertStatement>(
        collection->collection->id(),
        std::move(bound_values)
    );
}

} // namespace litedb::core::binder

#include "core/binder/worker/binder_worker_helper.hpp"

#include <expected>
#include <cstdint>
#include <exception>
#include <limits>
#include <memory>
#include <optional>
#include <string>
#include <unordered_set>
#include <utility>
#include <vector>

#include "core/binder/binder_helper.hpp"
#include "core/binder/bound/expression/bound_binary_expression.hpp"
#include "core/binder/bound/expression/bound_between_expression.hpp"
#include "core/binder/bound/expression/bound_column_ref_expression.hpp"
#include "core/binder/bound/expression/bound_function_expression.hpp"
#include "core/binder/bound/expression/bound_in_expression.hpp"
#include "core/binder/bound/expression/bound_like_expression.hpp"
#include "core/binder/bound/expression/bound_literal_expression.hpp"
#include "core/binder/bound/expression/bound_null_expression.hpp"
#include "core/binder/bound/expression/bound_unary_expression.hpp"
#include "core/binder/bound/expression/bound_vector_expression.hpp"
#include "core/common/identifier.hpp"
#include "core/schema/default_expression.hpp"
#include "core/parser/ast/expression/between_expression.hpp"
#include "core/parser/ast/expression/binary_expression.hpp"
#include "core/parser/ast/expression/column_reference_expression.hpp"
#include "core/parser/ast/expression/expression_node.hpp"
#include "core/parser/ast/expression/function_call_expression.hpp"
#include "core/parser/ast/expression/in_expression.hpp"
#include "core/parser/ast/expression/like_expression.hpp"
#include "core/parser/ast/expression/literal_expression.hpp"
#include "core/parser/ast/expression/unary_expression.hpp"
#include "core/parser/ast/expression/vector_expression.hpp"
#include "core/parser/ast/expression/wildcard_expression.hpp"

namespace litedb::core::binder
{

using namespace litedb::core::binder::bound;
using namespace litedb::core::common;
using namespace litedb::core::parser;
using namespace litedb::core::parser::ast;

namespace
{

common::BinaryOperator binary_operator(TokenType op)
{
    switch (op) {
    case TokenType::Plus:
        return common::BinaryOperator::Add;
    case TokenType::Minus:
        return common::BinaryOperator::Subtract;
    case TokenType::Star:
        return common::BinaryOperator::Multiply;
    case TokenType::Slash:
        return common::BinaryOperator::Divide;
    case TokenType::Modulo:
        return common::BinaryOperator::Modulus;
    case TokenType::Equal:
        return common::BinaryOperator::Equal;
    case TokenType::NotEqual:
        return common::BinaryOperator::NotEqual;
    case TokenType::LessThan:
        return common::BinaryOperator::LessThan;
    case TokenType::LessEqual:
        return common::BinaryOperator::LessThanOrEqual;
    case TokenType::GreaterThan:
        return common::BinaryOperator::GreaterThan;
    case TokenType::GreaterEqual:
        return common::BinaryOperator::GreaterThanOrEqual;
    case TokenType::And:
        return common::BinaryOperator::And;
    case TokenType::Or:
        return common::BinaryOperator::Or;
    default:
        std::unreachable();
    }
}

[[nodiscard]]
std::optional<common::Value> parse_literal_value(
    common::LogicalTypeId type_id,
    const std::string & text
)
{
    try {
        std::size_t parsed = 0;
        switch (type_id) {
        case LogicalTypeId::Boolean:
            if (text == "true" || text == "TRUE") {
                return common::Value {common::ValueData {true}};
            }
            if (text == "false" || text == "FALSE") {
                return common::Value {common::ValueData {false}};
            }
            break;
        case LogicalTypeId::Integer: {
            const auto value = std::stoll(text, &parsed);
            if (parsed == text.size()
                && value >= std::numeric_limits<std::int32_t>::min()
                && value <= std::numeric_limits<std::int32_t>::max()) {
                return common::Value {
                    common::ValueData {static_cast<std::int32_t>(value)}
                };
            }
            break;
        }
        case LogicalTypeId::BigInt: {
            const auto value = std::stoll(text, &parsed);
            if (parsed == text.size()) {
                return common::Value {common::ValueData {static_cast<std::int64_t>(value)}};
            }
            break;
        }
        case LogicalTypeId::Float: {
            const auto value = std::stof(text, &parsed);
            if (parsed == text.size()) {
                return common::Value {common::ValueData {value}};
            }
            break;
        }
        case LogicalTypeId::Double: {
            const auto value = std::stod(text, &parsed);
            if (parsed == text.size()) {
                return common::Value {common::ValueData {value}};
            }
            break;
        }
        case LogicalTypeId::Varchar:
            return common::Value {common::ValueData {text}};
        case LogicalTypeId::Null:
            return common::Value::null();
        case LogicalTypeId::Vector:
            break;
        }
    } catch (const std::exception &) {
    }
    return std::nullopt;
}

} // namespace

BinderWorkerHelper::BinderWorkerHelper(const BinderContext & context)
    : context_(context)
{
}

std::expected<DatabaseId, BinderError> BinderWorkerHelper::require_database() const
{
    if (!context_.session().current_database_id.has_value()) [[unlikely]] {
        return std::unexpected(make_binder_error(
            BinderErrorCode::DatabaseNotSelected,
            "No database selected"
        ));
    }

    if (context_.meta().find_database(context_.session().current_database_id.value()) == nullptr) [[unlikely]] {
        return std::unexpected(make_binder_error(
            BinderErrorCode::DatabaseNotFound,
            "Current database not found"
        ));
    }

    return context_.session().current_database_id.value();
}

std::expected<BindingCollection, BinderError> BinderWorkerHelper::bind_collection(
    const std::string & collection_name
) const
{
    auto database_id = require_database();
    if (!database_id.has_value()) [[unlikely]] {
        return std::unexpected(std::move(database_id.error()));
    }

    const auto * collection = context_.meta().find_collection(*database_id, collection_name);
    if (collection == nullptr) [[unlikely]] {
        return std::unexpected(make_binder_error(
            BinderErrorCode::CollectionNotFound,
            "Collection not found: " + collection_name
        ));
    }

    return BindingCollection {
        .database_id = *database_id,
        .collection = collection,
    };
}

std::expected<std::unique_ptr<BoundExpression>, BinderError> BinderWorkerHelper::bind_expression(
    const ExpressionNode & expression, const BindingCollection & collection
) const
{
    switch (expression.kind()) {
    case AstNodeKind::Literal:
        return bind_literal(static_cast<const LiteralExpression &>(expression));
    case AstNodeKind::ColumnReference:
        return bind_column_reference(static_cast<const ColumnReferenceExpression &>(expression), collection);
    case AstNodeKind::Unary:
        return bind_unary(static_cast<const UnaryExpression &>(expression), collection);
    case AstNodeKind::Binary:
        return bind_binary(static_cast<const BinaryExpression &>(expression), collection);
    case AstNodeKind::Vector:
        return bind_vector(static_cast<const VectorExpression &>(expression), collection);
    case AstNodeKind::In:
        return bind_in(static_cast<const InExpression &>(expression), collection);
    case AstNodeKind::Between:
        return bind_between(static_cast<const BetweenExpression &>(expression), collection);
    case AstNodeKind::Like:
        return bind_like(static_cast<const LikeExpression &>(expression), collection);
    case AstNodeKind::Wildcard:
        return std::unexpected(make_binder_error(
            BinderErrorCode::UnsupportedExpression,
            expression.location(),
            "Wildcard is only valid as a SELECT projection"
        ));
    case AstNodeKind::FunctionCall:
        return bind_function(static_cast<const FunctionCallExpression &>(expression), collection);
    [[unlikely]] default:
        return std::unexpected(make_binder_error(
            BinderErrorCode::UnsupportedExpression,
            "Unsupported expression"
        ));
    }
}

std::expected<std::unique_ptr<BoundExpression>, BinderError> BinderWorkerHelper::bind_literal(
    const LiteralExpression & expression
) const
{
    auto make_literal = [&expression](LogicalTypeId type_id)
        -> std::expected<std::unique_ptr<BoundExpression>, BinderError> {
        auto value = parse_literal_value(type_id, expression.value());
        if (!value.has_value()) {
            return std::unexpected(make_binder_error(
                BinderErrorCode::InvalidLiteral,
                expression.location(),
                "Invalid literal: " + expression.value()
            ));
        }
        return std::make_unique<BoundLiteralExpression>(
            type(type_id),
            std::move(*value)
        );
    };

    switch (expression.literal_type()) {
    case TokenType::Null:
        return std::make_unique<BoundNullExpression>(type(LogicalTypeId::Null));
    case TokenType::True:
    case TokenType::False:
        return make_literal(LogicalTypeId::Boolean);
    case TokenType::IntegerLiteral:
        return make_literal(LogicalTypeId::Integer);
    case TokenType::FloatLiteral:
        return make_literal(LogicalTypeId::Double);
    case TokenType::StringLiteral:
        return make_literal(LogicalTypeId::Varchar);
    [[unlikely]] default:
        return std::unexpected(make_binder_error(
            BinderErrorCode::InvalidType,
            "Unsupported literal"
        ));
    }
}

std::expected<std::unique_ptr<BoundExpression>, BinderError> BinderWorkerHelper::bind_column_reference(
    const ColumnReferenceExpression & expression, const BindingCollection & collection
) const
{
    // 检查限定符是否匹配集合
    if (expression.qualifier().has_value()
        && common::normalize_identifier(expression.qualifier().value()) != collection.collection->key()) [[unlikely]] {
        return std::unexpected(make_binder_error(
            BinderErrorCode::InvalidQualifier,
            "Column qualifier does not match FROM collection: " + expression.qualifier().value()
        ));
    }

    const auto * column = context_.meta().find_column(
        collection.collection->id(),
        expression.column_name()
    );
    if (column == nullptr) [[unlikely]] {
        return std::unexpected(make_binder_error(
            BinderErrorCode::ColumnNotFound,
            "Column not found: " + expression.column_name()
        ));
    }

    return std::make_unique<BoundColumnRefExpression>(
        column->id(),
        column->ordinal(),
        column->type()
    );
}

std::expected<std::unique_ptr<BoundExpression>, BinderError> BinderWorkerHelper::bind_function(
    const FunctionCallExpression & expression, const BindingCollection & collection
) const
{
    std::vector<std::unique_ptr<BoundExpression>> arguments;
    std::vector<LogicalType> argument_types;
    arguments.reserve(expression.arguments().size());
    argument_types.reserve(expression.arguments().size());

    for (const auto & argument : expression.arguments()) {
        auto bound_argument = bind_expression(*argument, collection);
        if (!bound_argument.has_value()) [[unlikely]] {
            return std::unexpected(std::move(bound_argument.error()));
        }
        argument_types.push_back((*bound_argument)->type());
        arguments.push_back(std::move(*bound_argument));
    }

    auto binding = context_.functions().bind_scalar(expression.name(), argument_types);
    if (!binding.has_value()) [[unlikely]] {
        const auto code = binding.error().is(function::FunctionErrorCode::FunctionNotFound)
            ? BinderErrorCode::FunctionNotFound
            : binding.error().is(function::FunctionErrorCode::AmbiguousOverload)
                ? BinderErrorCode::AmbiguousFunctionCall
                : binding.error().is(function::FunctionErrorCode::ConstraintViolation)
                    ? BinderErrorCode::InvalidType
                    : BinderErrorCode::NoMatchingFunctionOverload;
        const auto message = binding.error().message();
        auto cause = std::move(binding.error());
        return std::unexpected(BinderError {
            code,
            message,
            BinderErrorContext {expression.location()},
            std::move(cause),
        });
    }

    for (std::size_t index = 0; index < arguments.size(); ++index) {
        const auto & target_type = binding->argument_types()[index];
        if (!can_cast(arguments[index]->type(), target_type)) [[unlikely]] {
            return std::unexpected(make_binder_error(
                BinderErrorCode::InvalidType,
                expression.arguments()[index]->location(),
                "Function argument type " + type_name(arguments[index]->type())
                    + " cannot be cast to " + type_name(target_type)
            ));
        }
        if (!(arguments[index]->type().id == LogicalTypeId::Vector
            && target_type.id == LogicalTypeId::Vector
            && !target_type.parameter.has_value())) {
            arguments[index] = cast_if_needed(std::move(arguments[index]), target_type);
        }
    }

    return std::make_unique<BoundFunctionExpression>(
        std::move(*binding),
        std::move(arguments)
    );
}

std::expected<std::unique_ptr<BoundExpression>, BinderError> BinderWorkerHelper::bind_unary(
    const UnaryExpression & expression, const BindingCollection & collection
) const
{
    auto operand = bind_expression(expression.operand(), collection);
    if (!operand.has_value()) [[unlikely]] {
        return std::unexpected(std::move(operand.error()));
    }

    if (expression.op() == TokenType::Not) {
        if (!is_boolean((*operand)->type())) [[unlikely]] {
            return std::unexpected(make_binder_error(
                BinderErrorCode::InvalidType,
                expression.location(),
                "NOT operand must be BOOLEAN"
            ));
        }
        return std::make_unique<BoundUnaryExpression>(
            common::UnaryOperator::Not,
            std::move(*operand),
            type(LogicalTypeId::Boolean)
        );
    }

    if ((expression.op() == TokenType::Plus || expression.op() == TokenType::Minus)
        && is_numeric((*operand)->type())) {
        if (expression.op() == TokenType::Plus) {
            return std::move(*operand);
        }
        const auto result_type = (*operand)->type();
        return std::make_unique<BoundUnaryExpression>(
            common::UnaryOperator::Negate,
            std::move(*operand),
            result_type
        );
    }

    [[unlikely]] return std::unexpected(make_binder_error(
        BinderErrorCode::InvalidType,
        expression.location(),
        "Invalid unary operand type"
    ));
}

std::expected<std::unique_ptr<BoundExpression>, BinderError> BinderWorkerHelper::bind_binary(
    const BinaryExpression & expression, const BindingCollection & collection
) const
{
    auto left = bind_expression(expression.left(), collection);
    if (!left.has_value()) [[unlikely]] {
        return std::unexpected(std::move(left.error()));
    }

    auto right = bind_expression(expression.right(), collection);
    if (!right.has_value()) [[unlikely]] {
        return std::unexpected(std::move(right.error()));
    }

    const auto token_op = expression.op();
    if (token_op == TokenType::And || token_op == TokenType::Or) {
        if (!is_boolean((*left)->type()) || !is_boolean((*right)->type())) [[unlikely]] {
            return std::unexpected(make_binder_error(
                BinderErrorCode::InvalidType,
                expression.location(),
                "Logical operands must be BOOLEAN"
            ));
        }

        return std::make_unique<BoundBinaryExpression>(
            std::move(*left),
            binary_operator(token_op),
            std::move(*right),
            type(LogicalTypeId::Boolean)
        );
    }

    if (token_op == TokenType::Plus || token_op == TokenType::Minus || token_op == TokenType::Star
        || token_op == TokenType::Slash || token_op == TokenType::Modulo) {
        if (!is_numeric((*left)->type()) || !is_numeric((*right)->type())) [[unlikely]] {
            return std::unexpected(make_binder_error(
                BinderErrorCode::InvalidType,
                expression.location(),
                "Arithmetic operands must be numeric"
            ));
        }
        const auto result_type = common_numeric_type((*left)->type(), (*right)->type());
        return std::make_unique<BoundBinaryExpression>(
            cast_if_needed(std::move(*left), result_type),
            binary_operator(token_op),
            cast_if_needed(std::move(*right), result_type),
            result_type
        );
    }

    if (token_op == TokenType::Equal || token_op == TokenType::NotEqual || token_op == TokenType::LessThan
        || token_op == TokenType::LessEqual || token_op == TokenType::GreaterThan || token_op == TokenType::GreaterEqual) {
        const auto op = binary_operator(token_op);
        if (!can_compare((*left)->type(), (*right)->type(), op)) [[unlikely]] {
            return std::unexpected(make_binder_error(
                BinderErrorCode::InvalidType,
                expression.location(),
                "Comparison operands are not compatible"
            ));
        }
        if (is_numeric((*left)->type()) && is_numeric((*right)->type())) {
            const auto common_type = common_numeric_type((*left)->type(), (*right)->type());
            left = cast_if_needed(std::move(*left), common_type);
            right = cast_if_needed(std::move(*right), common_type);
        }
        return std::make_unique<BoundBinaryExpression>(
            std::move(*left),
            op,
            std::move(*right),
            type(LogicalTypeId::Boolean)
        );
    }

    [[unlikely]] return std::unexpected(make_binder_error(
        BinderErrorCode::UnsupportedExpression,
        expression.location(),
        "Unsupported binary operator"
    ));
}

std::expected<std::unique_ptr<BoundExpression>, BinderError> BinderWorkerHelper::bind_vector(
    const VectorExpression & expression, const BindingCollection & collection
) const
{
    std::vector<std::unique_ptr<BoundExpression>> elements;
    elements.reserve(expression.elements().size());

    for (const auto & element : expression.elements()) {
        auto bound_element = bind_expression(*element, collection);
        if (!bound_element.has_value()) [[unlikely]] {
            return std::unexpected(std::move(bound_element.error()));
        }
        if (!is_numeric((*bound_element)->type())) [[unlikely]] {
            return std::unexpected(make_binder_error(
                BinderErrorCode::InvalidType,
                "Vector elements must be numeric"
            ));
        }
        elements.push_back(cast_if_needed(std::move(*bound_element), type(LogicalTypeId::Double)));
    }

    return std::make_unique<BoundVectorExpression>(
        std::move(elements)
    );
}

std::expected<std::unique_ptr<BoundExpression>, BinderError> BinderWorkerHelper::bind_in(
    const InExpression & expression, const BindingCollection & collection
) const
{
    auto target = bind_expression(expression.expression(), collection);
    if (!target.has_value()) [[unlikely]] {
        return std::unexpected(std::move(target.error()));
    }

    std::vector<std::unique_ptr<BoundExpression>> values;
    auto comparison_type = (*target)->type();
    for (const auto & value : expression.values()) {
        auto bound_value = bind_expression(*value, collection);
        if (!bound_value.has_value()) [[unlikely]] {
            return std::unexpected(std::move(bound_value.error()));
        }
        if (!can_compare(
            (*target)->type(),
            (*bound_value)->type(),
            common::BinaryOperator::Equal
        )) [[unlikely]] {
            return std::unexpected(make_binder_error(
                BinderErrorCode::InvalidType,
                value->location(),
                "IN value is not comparable with target expression"
            ));
        }
        if (is_numeric(comparison_type) && is_numeric((*bound_value)->type())) {
            comparison_type = common_numeric_type(
                comparison_type,
                (*bound_value)->type()
            );
        }
        values.push_back(std::move(*bound_value));
    }

    if (is_numeric(comparison_type)) {
        target = cast_if_needed(std::move(*target), comparison_type);
        for (auto & value : values) {
            value = cast_if_needed(std::move(value), comparison_type);
        }
    }

    return std::make_unique<BoundInExpression>(std::move(*target), std::move(values));
}

std::expected<std::unique_ptr<BoundExpression>, BinderError> BinderWorkerHelper::bind_between(
    const BetweenExpression & expression, const BindingCollection & collection
) const
{
    auto target = bind_expression(expression.expression(), collection);
    if (!target.has_value()) [[unlikely]] {
        return std::unexpected(std::move(target.error()));
    }
    auto lower = bind_expression(expression.lower(), collection);
    if (!lower.has_value()) [[unlikely]] {
        return std::unexpected(std::move(lower.error()));
    }
    auto upper = bind_expression(expression.upper(), collection);
    if (!upper.has_value()) [[unlikely]] {
        return std::unexpected(std::move(upper.error()));
    }

    if (!can_compare(
            (*target)->type(),
            (*lower)->type(),
            common::BinaryOperator::GreaterThanOrEqual
        )
        || !can_compare(
            (*target)->type(),
            (*upper)->type(),
            common::BinaryOperator::LessThanOrEqual
        )) [[unlikely]] {
        return std::unexpected(make_binder_error(
            BinderErrorCode::InvalidType,
            expression.location(),
            "BETWEEN bounds are not comparable with target expression"
        ));
    }

    if (is_numeric((*target)->type())
        && is_numeric((*lower)->type())
        && is_numeric((*upper)->type())) {
        const auto comparison_type = common_numeric_type(
            common_numeric_type((*target)->type(), (*lower)->type()),
            (*upper)->type()
        );
        target = cast_if_needed(std::move(*target), comparison_type);
        lower = cast_if_needed(std::move(*lower), comparison_type);
        upper = cast_if_needed(std::move(*upper), comparison_type);
    }

    return std::make_unique<BoundBetweenExpression>(
        std::move(*target),
        std::move(*lower),
        std::move(*upper)
    );
}

std::expected<std::unique_ptr<BoundExpression>, BinderError> BinderWorkerHelper::bind_like(
    const LikeExpression & expression, const BindingCollection & collection
) const
{
    auto target = bind_expression(expression.expression(), collection);
    if (!target.has_value()) [[unlikely]] {
        return std::unexpected(std::move(target.error()));
    }
    auto pattern = bind_expression(expression.pattern(), collection);
    if (!pattern.has_value()) [[unlikely]] {
        return std::unexpected(std::move(pattern.error()));
    }

    if (!is_varchar((*target)->type()) || !is_varchar((*pattern)->type())) [[unlikely]] {
        return std::unexpected(make_binder_error(
            BinderErrorCode::InvalidType,
            expression.location(),
            "LIKE operands must be VARCHAR"
        ));
    }

    return std::make_unique<BoundLikeExpression>(
        std::move(*target),
        std::move(*pattern)
    );
}

std::expected<std::vector<std::unique_ptr<BoundExpression>>, BinderError> BinderWorkerHelper::expand_wildcard(
    const WildcardExpression & expression, const BindingCollection & collection
) const
{
    if (expression.qualifier().has_value()
        && common::normalize_identifier(expression.qualifier().value()) != collection.collection->key()) [[unlikely]] {
        return std::unexpected(make_binder_error(
            BinderErrorCode::InvalidQualifier,
            expression.location(),
            "Wildcard qualifier does not match FROM collection: " + expression.qualifier().value()
        ));
    }

    std::vector<std::unique_ptr<BoundExpression>> expressions;
    for (const auto * column : context_.meta().list_columns(collection.collection->id())) {
        expressions.push_back(std::make_unique<BoundColumnRefExpression>(
            column->id(),
            column->ordinal(),
            column->type()
        ));
    }
    return expressions;
}

std::expected<std::unique_ptr<BoundExpression>, BinderError> BinderWorkerHelper::bind_default_expression(
    const schema::DefaultExpression & expression
) const
{
    if (expression.kind == schema::DefaultExpressionKind::Vector) {
        std::vector<std::unique_ptr<BoundExpression>> elements;
        elements.reserve(expression.elements.size());
        for (const auto & element : expression.elements) {
            auto bound_element = bind_default_expression(element);
            if (!bound_element.has_value()) [[unlikely]] {
                return std::unexpected(std::move(bound_element.error()));
            }
            if (!is_numeric((*bound_element)->type())) [[unlikely]] {
                return std::unexpected(make_binder_error(
                    BinderErrorCode::InvalidType,
                    "Vector default elements must be numeric"
                ));
            }
            elements.push_back(cast_if_needed(std::move(*bound_element), type(LogicalTypeId::Double)));
        }
        return std::make_unique<BoundVectorExpression>(
            std::move(elements)
        );
    }

    auto make_literal = [&expression](LogicalTypeId type_id)
        -> std::expected<std::unique_ptr<BoundExpression>, BinderError> {
        auto value = parse_literal_value(type_id, expression.value);
        if (!value.has_value()) {
            return std::unexpected(make_binder_error(
                BinderErrorCode::InvalidLiteral,
                "Invalid default literal: " + expression.value
            ));
        }
        return std::make_unique<BoundLiteralExpression>(
            type(type_id),
            std::move(*value)
        );
    };

    switch (expression.literal_kind) {
    case schema::DefaultLiteralKind::Null:
        return std::make_unique<BoundNullExpression>(type(LogicalTypeId::Null));
    case schema::DefaultLiteralKind::Boolean:
        return make_literal(LogicalTypeId::Boolean);
    case schema::DefaultLiteralKind::Integer:
        return make_literal(LogicalTypeId::Integer);
    case schema::DefaultLiteralKind::Float:
        return make_literal(LogicalTypeId::Double);
    case schema::DefaultLiteralKind::String:
        return make_literal(LogicalTypeId::Varchar);
    }

    [[unlikely]] return std::unexpected(make_binder_error(
        BinderErrorCode::InvalidType,
        "Unsupported default expression"
    ));
}

std::expected<std::vector<meta::ColumnDefinition>, BinderError> BinderWorkerHelper::bind_column_definitions(
    const ColumnDefinitionSyntaxList & columns
) const
{
    std::unordered_set<std::string> seen_columns;
    std::vector<meta::ColumnDefinition> result;
    result.reserve(columns.size());

    for (const auto & column_ptr : columns) {
        const auto & column = *column_ptr;
        if (!seen_columns.emplace(common::normalize_identifier(column.name)).second) [[unlikely]] {
            return std::unexpected(make_binder_error(
                BinderErrorCode::DuplicateColumn,
                column.location,
                "Duplicate column: " + column.name
            ));
        }
        auto logical_type = validate_data_type(column.type);
        if (!logical_type.has_value()) [[unlikely]] {
            return std::unexpected(std::move(logical_type.error()));
        }

        std::optional<schema::DefaultExpression> default_expression;
        if (column.default_value != nullptr) {
            auto default_snapshot = snapshot_default_expression(*column.default_value);
            if (!default_snapshot.has_value()) [[unlikely]] {
                return std::unexpected(std::move(default_snapshot.error()));
            }
            default_expression = std::move(*default_snapshot);

            auto bound_default = bind_default_expression(*default_expression);
            if (!bound_default.has_value()) [[unlikely]] {
                return std::unexpected(std::move(bound_default.error()));
            }
            if ((*bound_default)->type().id == LogicalTypeId::Null
                && !column.nullable) [[unlikely]] {
                return std::unexpected(make_binder_error(
                    BinderErrorCode::NotNullable,
                    column.default_value->location(),
                    "NOT NULL column cannot have a NULL default: " + column.name
                ));
            }
            if (!can_cast((*bound_default)->type(), *logical_type)) [[unlikely]] {
                return std::unexpected(make_binder_error(
                    BinderErrorCode::InvalidType,
                    column.default_value->location(),
                    "DEFAULT value type " + type_name((*bound_default)->type())
                        + " does not match column " + column.name
                        + " type " + type_name(*logical_type)
                ));
            }
        }

        result.push_back(meta::ColumnDefinition {
            .name = column.name,
            .type = *logical_type,
            .unique = column.unique,
            .nullable = column.nullable,
            .default_expression = std::move(default_expression),
            .comment = column.comment,
        });
    }

    return result;
}

std::expected<LogicalType, BinderError> BinderWorkerHelper::validate_data_type(
    const LogicalType & data_type
) const
{
    switch (data_type.id) {
    case LogicalTypeId::Null:
        return std::unexpected(make_binder_error(BinderErrorCode::InvalidType, "NULL is not a valid column type"));
    case LogicalTypeId::Integer:
    case LogicalTypeId::BigInt:
    case LogicalTypeId::Float:
    case LogicalTypeId::Double:
    case LogicalTypeId::Boolean:
        if (data_type.parameter.has_value()) [[unlikely]] {
            return std::unexpected(make_binder_error(BinderErrorCode::InvalidType, "Scalar type cannot have a parameter"));
        }
        return data_type;
    case LogicalTypeId::Varchar:
        if (!data_type.parameter.has_value() || data_type.parameter.value() == 0) [[unlikely]] {
            return std::unexpected(make_binder_error(BinderErrorCode::InvalidType, "VARCHAR length must be positive"));
        }
        return data_type;
    case LogicalTypeId::Vector:
        if (!data_type.parameter.has_value() || data_type.parameter.value() == 0) [[unlikely]] {
            return std::unexpected(make_binder_error(BinderErrorCode::InvalidType, "VECTOR dimension must be positive"));
        }
        return data_type;
    }
    [[unlikely]] return std::unexpected(make_binder_error(BinderErrorCode::InvalidType, "Unsupported data type"));
}

std::expected<schema::DefaultExpression, BinderError> BinderWorkerHelper::snapshot_default_expression(
    const ExpressionNode & expression
) const
{
    if (expression.kind() == AstNodeKind::Literal) {
        const auto & literal = static_cast<const LiteralExpression &>(expression);
        switch (literal.literal_type()) {
        case TokenType::Null:
            return schema::DefaultExpression::null_literal();
        case TokenType::True:
            [[fallthrough]];
        case TokenType::False:
            return schema::DefaultExpression::literal(
                schema::DefaultLiteralKind::Boolean,
                literal.value()
            );
        case TokenType::IntegerLiteral:
            return schema::DefaultExpression::literal(
                schema::DefaultLiteralKind::Integer,
                literal.value()
            );
        case TokenType::FloatLiteral:
            return schema::DefaultExpression::literal(
                schema::DefaultLiteralKind::Float,
                literal.value()
            );
        case TokenType::StringLiteral:
            return schema::DefaultExpression::literal(
                schema::DefaultLiteralKind::String,
                literal.value()
            );
        default:
            break;
        }
    }

    if (expression.kind() == AstNodeKind::Vector) {
        const auto & vector = static_cast<const VectorExpression &>(expression);
        std::vector<schema::DefaultExpression> elements;
        elements.reserve(vector.elements().size());
        for (const auto & element : vector.elements()) {
            auto snapshot = snapshot_default_expression(*element);
            if (!snapshot.has_value()) [[unlikely]] {
                return std::unexpected(std::move(snapshot.error()));
            }
            elements.push_back(std::move(*snapshot));
        }
        return schema::DefaultExpression::vector(std::move(elements));
    }

    [[unlikely]] return std::unexpected(make_binder_error(
        BinderErrorCode::InvalidType,
        expression.location(),
        "Unsupported default expression"
    ));
}

} // namespace litedb::core::binder

#include "core/binder/worker/binder_worker_helper.hpp"

#include <expected>
#include <memory>
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
#include "core/binder/bound/expression/bound_wildcard_expression.hpp"
#include "core/catalog/catalog_default_expression.hpp"
#include "core/catalog/catalog_entry.hpp"
#include "core/function/builtin/builtin_functions.hpp"
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

BinderWorkerHelper::BinderWorkerHelper(const BinderContext & context)
    : context_(context)
{
}

std::expected<DatabaseId, BinderError> BinderWorkerHelper::require_database(AstNodeLocation location) const
{
    if (!context_.session().current_database_id.has_value()) [[unlikely]] {
        return std::unexpected(make_binder_error(
            BinderErrorCode::DatabaseNotSelected,
            location,
            "No database selected"
        ));
    }

    if (context_.catalog().find_database(context_.session().current_database_id.value()) == nullptr) [[unlikely]] {
        return std::unexpected(make_binder_error(
            BinderErrorCode::DatabaseNotFound,
            location,
            "Current database not found"
        ));
    }

    return context_.session().current_database_id.value();
}

std::expected<BindingCollection, BinderError> BinderWorkerHelper::bind_collection(
    const std::string & collection_name, AstNodeLocation location
) const
{
    const auto database_id = require_database(location);
    if (!database_id.has_value()) [[unlikely]] {
        return std::unexpected(database_id.error());
    }

    const auto * collection = context_.catalog().find_collection(database_id.value(), collection_name);
    if (collection == nullptr) [[unlikely]] {
        return std::unexpected(make_binder_error(
            BinderErrorCode::CollectionNotFound,
            location,
            "Collection not found: " + collection_name
        ));
    }

    return BindingCollection {
        .database_id = database_id.value(),
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
        return std::make_unique<BoundWildcardExpression>(
            static_cast<const WildcardExpression &>(expression).qualifier(),
            expression.location()
        );
    case AstNodeKind::FunctionCall:
        return bind_function(static_cast<const FunctionCallExpression &>(expression), collection);
    [[unlikely]] default:
        return std::unexpected(make_binder_error(
            BinderErrorCode::UnsupportedExpression,
            expression.location(),
            "Unsupported expression"
        ));
    }
}

std::expected<std::unique_ptr<BoundExpression>, BinderError> BinderWorkerHelper::bind_literal(
    const LiteralExpression & expression
) const
{
    switch (expression.literal_type()) {
    case TokenType::Null:
        return std::make_unique<BoundNullExpression>(type(LogicalTypeId::Null), expression.location());
    case TokenType::True:
    case TokenType::False:
        return std::make_unique<BoundLiteralExpression>(type(LogicalTypeId::Boolean), expression.value(), expression.location());
    case TokenType::IntegerLiteral:
        return std::make_unique<BoundLiteralExpression>(type(LogicalTypeId::Integer), expression.value(), expression.location());
    case TokenType::FloatLiteral:
        return std::make_unique<BoundLiteralExpression>(type(LogicalTypeId::Double), expression.value(), expression.location());
    case TokenType::StringLiteral:
        return std::make_unique<BoundLiteralExpression>(type(LogicalTypeId::Varchar), expression.value(), expression.location());
    [[unlikely]] default:
        return std::unexpected(make_binder_error(
            BinderErrorCode::InvalidType,
            expression.location(),
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
        && catalog::normalize_identifier(expression.qualifier().value()) != collection.collection->key()) [[unlikely]] {
        return std::unexpected(make_binder_error(
            BinderErrorCode::InvalidQualifier,
            expression.location(),
            "Column qualifier does not match FROM collection: " + expression.qualifier().value()
        ));
    }

    const auto * column = context_.catalog().find_column(collection.collection->id(), expression.column());
    if (column == nullptr) [[unlikely]] {
        return std::unexpected(make_binder_error(
            BinderErrorCode::ColumnNotFound,
            expression.location(),
            "Column not found: " + expression.column()
        ));
    }

    return std::make_unique<BoundColumnRefExpression>(
        collection.database_id,
        collection.collection->id(),
        collection.collection->name(),
        column->id(),
        column->name(),
        column->type(),
        column->nullable(),
        expression.location()
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
        argument_types.push_back(bound_argument.value()->type());
        arguments.push_back(std::move(bound_argument.value()));
    }

    auto registry = function::builtin::make_builtin_function_registry();
    auto binding = registry.bind_scalar(expression.name(), argument_types);
    if (!binding.has_value()) [[unlikely]] {
        const auto found = registry.find(expression.name());
        return std::unexpected(make_binder_error(
            found == nullptr ? BinderErrorCode::UnsupportedExpression : BinderErrorCode::InvalidType,
            expression.location(),
            found == nullptr ? "Unknown function: " + expression.name() : "Function arguments do not match any overload: " + expression.name()
        ));
    }

    if ((function::normalize_function_name(expression.name()) == "l2_distance"
        || function::normalize_function_name(expression.name()) == "cosine_distance"
        || function::normalize_function_name(expression.name()) == "inner_product")
        && argument_types.size() == 2
        && argument_types[0].id == LogicalTypeId::Vector
        && argument_types[1].id == LogicalTypeId::Vector
        && argument_types[0].parameter.has_value()
        && argument_types[1].parameter.has_value()
        && argument_types[0].parameter.value() != argument_types[1].parameter.value()) [[unlikely]] {
        return std::unexpected(make_binder_error(
            BinderErrorCode::InvalidType,
            expression.location(),
            "Vector function arguments must have the same dimension"
        ));
    }

    for (std::size_t index = 0; index < arguments.size(); ++index) {
        const auto signature_index = std::min(index, binding->signature.argument_types.size() - 1);
        const auto & target_type = binding->signature.argument_types[signature_index];
        if (!can_cast(arguments[index]->type(), target_type)) [[unlikely]] {
            return std::unexpected(make_binder_error(
                BinderErrorCode::InvalidType,
                arguments[index]->location(),
                "Function argument type " + type_name(arguments[index]->type())
                    + " cannot be cast to " + type_name(target_type)
            ));
        }
        if (!(arguments[index]->type().id == LogicalTypeId::Vector && target_type.id == LogicalTypeId::Vector && !target_type.parameter.has_value())) {
            arguments[index] = cast_if_needed(std::move(arguments[index]), target_type);
        }
    }

    return std::make_unique<BoundFunctionExpression>(
        expression.name(),
        std::move(binding->function),
        std::move(binding->signature),
        std::move(arguments),
        binding->signature.return_type,
        expression.location()
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
        if (!is_boolean(operand.value()->type())) [[unlikely]] {
            return std::unexpected(make_binder_error(
                BinderErrorCode::InvalidType,
                expression.location(),
                "NOT operand must be BOOLEAN"
            ));
        }
        return std::make_unique<BoundUnaryExpression>(
            expression.op(),
            std::move(operand.value()),
            type(LogicalTypeId::Boolean),
            expression.location()
        );
    }

    if ((expression.op() == TokenType::Plus || expression.op() == TokenType::Minus)
        && is_numeric(operand.value()->type())) {
        const auto result_type = operand.value()->type();
        return std::make_unique<BoundUnaryExpression>(
            expression.op(),
            std::move(operand.value()),
            result_type,
            expression.location()
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

    const auto op = expression.op();
    if (op == TokenType::And || op == TokenType::Or) {
        if (!is_boolean(left.value()->type()) || !is_boolean(right.value()->type())) [[unlikely]] {
            return std::unexpected(make_binder_error(
                BinderErrorCode::InvalidType,
                expression.location(),
                "Logical operands must be BOOLEAN"
            ));
        }

        return std::make_unique<BoundBinaryExpression>(
            std::move(left.value()),
            op,
            std::move(right.value()),
            type(LogicalTypeId::Boolean),
            expression.location()
        );
    }

    if (op == TokenType::Plus || op == TokenType::Minus || op == TokenType::Star
        || op == TokenType::Slash || op == TokenType::Modulo) {
        if (!is_numeric(left.value()->type()) || !is_numeric(right.value()->type())) [[unlikely]] {
            return std::unexpected(make_binder_error(
                BinderErrorCode::InvalidType,
                expression.location(),
                "Arithmetic operands must be numeric"
            ));
        }
        const auto result_type = common_numeric_type(left.value()->type(), right.value()->type());
        return std::make_unique<BoundBinaryExpression>(
            cast_if_needed(std::move(left.value()), result_type),
            op,
            cast_if_needed(std::move(right.value()), result_type),
            result_type,
            expression.location()
        );
    }

    if (op == TokenType::Equal || op == TokenType::NotEqual || op == TokenType::LessThan
        || op == TokenType::LessEqual || op == TokenType::GreaterThan || op == TokenType::GreaterEqual) {
        if (!can_compare(left.value()->type(), right.value()->type(), op)) [[unlikely]] {
            return std::unexpected(make_binder_error(
                BinderErrorCode::InvalidType,
                expression.location(),
                "Comparison operands are not compatible"
            ));
        }
        if (is_numeric(left.value()->type()) && is_numeric(right.value()->type())) {
            const auto common_type = common_numeric_type(left.value()->type(), right.value()->type());
            left = cast_if_needed(std::move(left.value()), common_type);
            right = cast_if_needed(std::move(right.value()), common_type);
        }
        return std::make_unique<BoundBinaryExpression>(
            std::move(left.value()),
            op,
            std::move(right.value()),
            type(LogicalTypeId::Boolean),
            expression.location()
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
        if (!is_numeric(bound_element.value()->type())) [[unlikely]] {
            return std::unexpected(make_binder_error(
                BinderErrorCode::InvalidType,
                element->location(),
                "Vector elements must be numeric"
            ));
        }
        elements.push_back(cast_if_needed(std::move(bound_element.value()), type(LogicalTypeId::Double)));
    }

    return std::make_unique<BoundVectorExpression>(
        std::move(elements),
        type(LogicalTypeId::Vector, expression.elements().size()),
        expression.location()
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
    for (const auto & value : expression.values()) {
        auto bound_value = bind_expression(*value, collection);
        if (!bound_value.has_value()) [[unlikely]] {
            return std::unexpected(std::move(bound_value.error()));
        }
        if (!can_compare(target.value()->type(), bound_value.value()->type(), TokenType::Equal)) [[unlikely]] {
            return std::unexpected(make_binder_error(
                BinderErrorCode::InvalidType,
                value->location(),
                "IN value is not comparable with target expression"
            ));
        }
        values.push_back(std::move(bound_value.value()));
    }

    return std::make_unique<BoundInExpression>(std::move(target.value()), std::move(values), expression.location());
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

    if (!can_compare(target.value()->type(), lower.value()->type(), TokenType::GreaterEqual)
        || !can_compare(target.value()->type(), upper.value()->type(), TokenType::LessEqual)) [[unlikely]] {
        return std::unexpected(make_binder_error(
            BinderErrorCode::InvalidType,
            expression.location(),
            "BETWEEN bounds are not comparable with target expression"
        ));
    }

    return std::make_unique<BoundBetweenExpression>(
        std::move(target.value()),
        std::move(lower.value()),
        std::move(upper.value()),
        expression.location()
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

    if (!is_varchar(target.value()->type()) || !is_varchar(pattern.value()->type())) [[unlikely]] {
        return std::unexpected(make_binder_error(
            BinderErrorCode::InvalidType,
            expression.location(),
            "LIKE operands must be VARCHAR"
        ));
    }

    return std::make_unique<BoundLikeExpression>(
        std::move(target.value()),
        std::move(pattern.value()),
        expression.location()
    );
}

std::expected<std::vector<std::unique_ptr<BoundExpression>>, BinderError> BinderWorkerHelper::expand_wildcard(
    const WildcardExpression & expression, const BindingCollection & collection
) const
{
    if (expression.qualifier().has_value()
        && catalog::normalize_identifier(expression.qualifier().value()) != collection.collection->key()) [[unlikely]] {
        return std::unexpected(make_binder_error(
            BinderErrorCode::InvalidQualifier,
            expression.location(),
            "Wildcard qualifier does not match FROM collection: " + expression.qualifier().value()
        ));
    }

    std::vector<std::unique_ptr<BoundExpression>> expressions;
    for (const auto * column : context_.catalog().list_columns(collection.collection->id())) {
        expressions.push_back(std::make_unique<BoundColumnRefExpression>(
            collection.database_id,
            collection.collection->id(),
            collection.collection->name(),
            column->id(),
            column->name(),
            column->type(),
            column->nullable(),
            expression.location()
        ));
    }
    return expressions;
}

std::expected<std::unique_ptr<BoundExpression>, BinderError> BinderWorkerHelper::bind_default_expression(
    const catalog::CatalogDefaultExpression & expression, AstNodeLocation location
) const
{
    if (expression.kind == catalog::CatalogDefaultExpressionKind::Vector) {
        std::vector<std::unique_ptr<BoundExpression>> elements;
        elements.reserve(expression.elements.size());
        for (const auto & element : expression.elements) {
            auto bound_element = bind_default_expression(element, location);
            if (!bound_element.has_value()) [[unlikely]] {
                return std::unexpected(std::move(bound_element.error()));
            }
            if (!is_numeric(bound_element.value()->type())) [[unlikely]] {
                return std::unexpected(make_binder_error(
                    BinderErrorCode::InvalidType,
                    location,
                    "Vector default elements must be numeric"
                ));
            }
            elements.push_back(cast_if_needed(std::move(bound_element.value()), type(LogicalTypeId::Double)));
        }
        return std::make_unique<BoundVectorExpression>(
            std::move(elements),
            type(LogicalTypeId::Vector, expression.elements.size()),
            location
        );
    }

    switch (expression.literal_kind) {
    case catalog::CatalogDefaultLiteralKind::Null:
        return std::make_unique<BoundNullExpression>(type(LogicalTypeId::Null), location);
    case catalog::CatalogDefaultLiteralKind::Boolean:
        return std::make_unique<BoundLiteralExpression>(type(LogicalTypeId::Boolean), expression.value, location);
    case catalog::CatalogDefaultLiteralKind::Integer:
        return std::make_unique<BoundLiteralExpression>(type(LogicalTypeId::Integer), expression.value, location);
    case catalog::CatalogDefaultLiteralKind::Float:
        return std::make_unique<BoundLiteralExpression>(type(LogicalTypeId::Double), expression.value, location);
    case catalog::CatalogDefaultLiteralKind::String:
        return std::make_unique<BoundLiteralExpression>(type(LogicalTypeId::Varchar), expression.value, location);
    }

    [[unlikely]] return std::unexpected(make_binder_error(BinderErrorCode::InvalidType, location, "Unsupported default expression"));
}

std::expected<std::vector<catalog::ColumnDefinition>, BinderError> BinderWorkerHelper::bind_column_definitions(
    const ColumnDefinitionList & columns, AstNodeLocation location
) const
{
    std::unordered_set<std::string> seen_columns;
    std::vector<catalog::ColumnDefinition> result;
    result.reserve(columns.size());

    for (const auto & column : columns) {
        if (!seen_columns.emplace(catalog::normalize_identifier(column.name)).second) [[unlikely]] {
            return std::unexpected(make_binder_error(
                BinderErrorCode::DuplicateColumn,
                location,
                "Duplicate column: " + column.name
            ));
        }
        auto logical_type = bind_data_type(column.type, location);
        if (!logical_type.has_value()) [[unlikely]] {
            return std::unexpected(std::move(logical_type.error()));
        }

        std::optional<catalog::CatalogDefaultExpression> default_expression;
        if (column.default_value != nullptr) {
            auto default_snapshot = snapshot_default_expression(*column.default_value);
            if (!default_snapshot.has_value()) [[unlikely]] {
                return std::unexpected(std::move(default_snapshot.error()));
            }
            default_expression = std::move(default_snapshot.value());

            auto bound_default = bind_default_expression(default_expression.value(), column.default_value->location());
            if (!bound_default.has_value()) [[unlikely]] {
                return std::unexpected(std::move(bound_default.error()));
            }
            if (!can_cast(bound_default.value()->type(), logical_type.value())) [[unlikely]] {
                return std::unexpected(make_binder_error(
                    BinderErrorCode::InvalidType,
                    column.default_value->location(),
                    "DEFAULT value type " + type_name(bound_default.value()->type())
                        + " does not match column " + column.name
                        + " type " + type_name(logical_type.value())
                ));
            }
        }

        result.push_back(catalog::ColumnDefinition {
            .name = column.name,
            .type = logical_type.value(),
            .primary_key = false,
            .unique = column.unique,
            .nullable = column.nullable,
            .default_expression = std::move(default_expression),
            .comment = column.comment,
        });
    }

    return result;
}

std::expected<LogicalType, BinderError> BinderWorkerHelper::bind_data_type(
    const DataType & data_type, AstNodeLocation location
) const
{
    switch (data_type.kind) {
    case DataTypeKind::Integer:
        return type(LogicalTypeId::Integer);
    case DataTypeKind::BigInt:
        return type(LogicalTypeId::BigInt);
    case DataTypeKind::Float:
        return type(LogicalTypeId::Float);
    case DataTypeKind::Double:
        return type(LogicalTypeId::Double);
    case DataTypeKind::Boolean:
        return type(LogicalTypeId::Boolean);
    case DataTypeKind::Varchar:
        if (!data_type.parameter.has_value() || data_type.parameter.value() == 0) [[unlikely]] {
            return std::unexpected(make_binder_error(BinderErrorCode::InvalidType, location, "VARCHAR length must be positive"));
        }
        return type(LogicalTypeId::Varchar, data_type.parameter);
    case DataTypeKind::Vector:
        if (!data_type.parameter.has_value() || data_type.parameter.value() == 0) [[unlikely]] {
            return std::unexpected(make_binder_error(BinderErrorCode::InvalidType, location, "VECTOR dimension must be positive"));
        }
        return type(LogicalTypeId::Vector, data_type.parameter);
    }
    [[unlikely]] return std::unexpected(make_binder_error(BinderErrorCode::InvalidType, location, "Unsupported data type"));
}

std::expected<catalog::CatalogDefaultExpression, BinderError> BinderWorkerHelper::snapshot_default_expression(
    const ExpressionNode & expression
) const
{
    if (expression.kind() == AstNodeKind::Literal) {
        const auto & literal = static_cast<const LiteralExpression &>(expression);
        switch (literal.literal_type()) {
        case TokenType::Null:
            return catalog::CatalogDefaultExpression::null_literal();
        case TokenType::True:
            [[fallthrough]];
        case TokenType::False:
            return catalog::CatalogDefaultExpression::literal(
                catalog::CatalogDefaultLiteralKind::Boolean,
                literal.value()
            );
        case TokenType::IntegerLiteral:
            return catalog::CatalogDefaultExpression::literal(
                catalog::CatalogDefaultLiteralKind::Integer,
                literal.value()
            );
        case TokenType::FloatLiteral:
            return catalog::CatalogDefaultExpression::literal(
                catalog::CatalogDefaultLiteralKind::Float,
                literal.value()
            );
        case TokenType::StringLiteral:
            return catalog::CatalogDefaultExpression::literal(
                catalog::CatalogDefaultLiteralKind::String,
                literal.value()
            );
        default:
            break;
        }
    }

    if (expression.kind() == AstNodeKind::Vector) {
        const auto & vector = static_cast<const VectorExpression &>(expression);
        std::vector<catalog::CatalogDefaultExpression> elements;
        elements.reserve(vector.elements().size());
        for (const auto & element : vector.elements()) {
            auto snapshot = snapshot_default_expression(*element);
            if (!snapshot.has_value()) [[unlikely]] {
                return std::unexpected(std::move(snapshot.error()));
            }
            elements.push_back(std::move(snapshot.value()));
        }
        return catalog::CatalogDefaultExpression::vector(std::move(elements));
    }

    [[unlikely]] return std::unexpected(make_binder_error(
        BinderErrorCode::InvalidType,
        expression.location(),
        "Unsupported default expression"
    ));
}

} // namespace litedb::core::binder

#include "core/binder/detail/expression_binder.hpp"

#include <utility>
#include <vector>

#include "core/binder/binder_context.hpp"
#include "core/binder/binder_helper.hpp"
#include "core/binder/bound/expression/bound_column_ref_expression.hpp"
#include "core/binder/bound/expression/bound_function_expression.hpp"
#include "core/binder/bound/expression/bound_literal_expression.hpp"
#include "core/binder/bound/expression/bound_null_expression.hpp"
#include "core/binder/bound/expression/bound_vector_expression.hpp"
#include "core/binder/detail/literal_value_parser.hpp"
#include "core/common/identifier.hpp"

namespace litedb::core::binder::detail
{

using namespace litedb::core::binder::bound;
using namespace litedb::core::common;
using namespace litedb::core::parser;
using namespace litedb::core::parser::ast;

ExpressionBinder::ExpressionBinder(
    const BinderContext & context,
    const BindingCollection & collection
)
    : context_(context)
    , collection_(collection)
{}

std::expected<std::unique_ptr<bound::BoundExpression>, BinderError> ExpressionBinder::bind(
    const ExpressionNode & expression
)
{
    return dispatch_expression(expression);
}

std::expected<std::unique_ptr<bound::BoundExpression>, BinderError>
ExpressionBinder::visit_wildcard_expression(const WildcardExpression & expression)
{
    return std::unexpected(make_binder_error(
        BinderErrorCode::UnsupportedExpression,
        expression.location(),
        "Wildcard is only valid as a SELECT projection"
    ));
}

std::expected<std::unique_ptr<bound::BoundExpression>, BinderError>
ExpressionBinder::visit_alias_expression(const AliasExpression & expression)
{
    return std::unexpected(
        make_binder_error(BinderErrorCode::UnsupportedExpression, "Unsupported expression")
    );
}

std::expected<std::unique_ptr<bound::BoundExpression>, BinderError>
ExpressionBinder::visit_literal_expression(const LiteralExpression & expression)
{
    auto make_literal = [&expression](
                            LogicalTypeId type_id
                        ) -> std::expected<std::unique_ptr<BoundExpression>, BinderError> {
        auto value = parse_literal_value(type_id, expression.value());
        if (!value.has_value()) {
            return std::unexpected(make_binder_error(
                BinderErrorCode::InvalidLiteral,
                expression.location(),
                "Invalid literal: " + expression.value()
            ));
        }
        return std::make_unique<BoundLiteralExpression>(type(type_id), std::move(*value));
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
    [[unlikely]]
    default:
        return std::unexpected(
            make_binder_error(BinderErrorCode::InvalidType, "Unsupported literal")
        );
    }
}

std::expected<std::unique_ptr<bound::BoundExpression>, BinderError>
ExpressionBinder::visit_column_reference_expression(const ColumnReferenceExpression & expression)
{
    // 检查限定符是否匹配集合
    if (expression.qualifier().has_value() &&
        common::normalize_identifier(expression.qualifier().value()) !=
            collection_.collection->key()) [[unlikely]] {
        return std::unexpected(make_binder_error(
            BinderErrorCode::InvalidQualifier,
            "Column qualifier does not match FROM collection: " + expression.qualifier().value()
        ));
    }

    const auto column =
        context_.catalog().find_column(collection_.collection->id(), expression.column_name());
    if (!column) [[unlikely]] {
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

std::expected<std::unique_ptr<bound::BoundExpression>, BinderError>
ExpressionBinder::visit_function_call_expression(const FunctionCallExpression & expression)
{
    std::vector<std::unique_ptr<BoundExpression>> arguments;
    std::vector<LogicalType> argument_types;
    arguments.reserve(expression.arguments().size());
    argument_types.reserve(expression.arguments().size());

    for (const auto & argument : expression.arguments()) {
        auto bound_argument = bind(*argument);
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
        return std::unexpected(
            BinderError {
                code,
                message,
                BinderErrorContext {expression.location()},
                std::move(cause),
            }
        );
    }

    for (std::size_t index = 0; index < arguments.size(); ++index) {
        const auto & target_type = binding->argument_types()[index];
        if (!can_cast(arguments[index]->type(), target_type)) [[unlikely]] {
            return std::unexpected(make_binder_error(
                BinderErrorCode::InvalidType,
                expression.arguments()[index]->location(),
                "Function argument type " + type_name(arguments[index]->type()) +
                    " cannot be cast to " + type_name(target_type)
            ));
        }
        if (!(arguments[index]->type().id == LogicalTypeId::Vector &&
              target_type.id == LogicalTypeId::Vector && !target_type.parameter.has_value())) {
            arguments[index] = cast_if_needed(std::move(arguments[index]), target_type);
        }
    }

    return std::make_unique<BoundFunctionExpression>(std::move(*binding), std::move(arguments));
}

std::expected<std::unique_ptr<bound::BoundExpression>, BinderError>
ExpressionBinder::visit_vector_expression(const VectorExpression & expression)
{
    std::vector<std::unique_ptr<BoundExpression>> elements;
    elements.reserve(expression.elements().size());

    for (const auto & element : expression.elements()) {
        auto bound_element = bind(*element);
        if (!bound_element.has_value()) [[unlikely]] {
            return std::unexpected(std::move(bound_element.error()));
        }
        if (!is_numeric((*bound_element)->type())) [[unlikely]] {
            return std::unexpected(
                make_binder_error(BinderErrorCode::InvalidType, "Vector elements must be numeric")
            );
        }
        elements.push_back(cast_if_needed(std::move(*bound_element), type(LogicalTypeId::Double)));
    }

    return std::make_unique<BoundVectorExpression>(std::move(elements));
}

} // namespace litedb::core::binder::detail

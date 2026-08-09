#include "core/binder/detail/default_expression_binder.hpp"

#include <utility>
#include <vector>

#include "core/binder/binder_helper.hpp"
#include "core/binder/bound/expression/bound_literal_expression.hpp"
#include "core/binder/bound/expression/bound_null_expression.hpp"
#include "core/binder/bound/expression/bound_vector_expression.hpp"
#include "core/binder/detail/literal_value_parser.hpp"
#include "core/parser/ast/expression/expression_node.hpp"
#include "core/parser/ast/expression/literal_expression.hpp"
#include "core/parser/ast/expression/vector_expression.hpp"

namespace litedb::core::binder::detail
{

using namespace litedb::core::binder::bound;
using namespace litedb::core::common;
using namespace litedb::core::parser;
using namespace litedb::core::parser::ast;

std::expected<std::unique_ptr<BoundExpression>, BinderError> bind_default_expression(
    const schema::DefaultExpression & expression
)
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
            elements.push_back(
                cast_if_needed(std::move(*bound_element), type(LogicalTypeId::Double))
            );
        }
        return std::make_unique<BoundVectorExpression>(std::move(elements));
    }

    auto make_literal = [&expression](
                            LogicalTypeId type_id
                        ) -> std::expected<std::unique_ptr<BoundExpression>, BinderError> {
        auto value = parse_literal_value(type_id, expression.value);
        if (!value.has_value()) {
            return std::unexpected(make_binder_error(
                BinderErrorCode::InvalidLiteral,
                "Invalid default literal: " + expression.value
            ));
        }
        return std::make_unique<BoundLiteralExpression>(type(type_id), std::move(*value));
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

    [[unlikely]] return std::unexpected(
        make_binder_error(BinderErrorCode::InvalidType, "Unsupported default expression")
    );
}

std::expected<schema::DefaultExpression, BinderError> snapshot_default_expression(
    const ExpressionNode & expression
)
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

} // namespace litedb::core::binder::detail

#include "core/parser/worker/parser_expression_worker.hpp"

#include <optional>
#include <string>
#include <utility>
#include <vector>

#include "core/parser/ast/expression/between_expression.hpp"
#include "core/parser/ast/expression/binary_expression.hpp"
#include "core/parser/ast/expression/column_reference_expression.hpp"
#include "core/parser/ast/expression/function_call_expression.hpp"
#include "core/parser/ast/expression/in_expression.hpp"
#include "core/parser/ast/expression/like_expression.hpp"
#include "core/parser/ast/expression/literal_expression.hpp"
#include "core/parser/ast/expression/unary_expression.hpp"
#include "core/parser/ast/expression/vector_expression.hpp"
#include "core/parser/parser_limits.hpp"
#include "core/parser/token.hpp"

namespace litedb::core::parser
{

namespace
{

constexpr int OR_PRECEDENCE = 1;
constexpr int AND_PRECEDENCE = 2;
constexpr int COMPARISON_PRECEDENCE = 3;
constexpr int ADDITIVE_PRECEDENCE = 4;
constexpr int MULTIPLICATIVE_PRECEDENCE = 5;

std::size_t max_depth(std::size_t left, std::size_t right) noexcept
{
    return left > right ? left : right;
}

std::optional<int> infix_precedence(TokenType type) noexcept
{
    switch (type) {
    case TokenType::Or:
        return OR_PRECEDENCE;
    case TokenType::And:
        return AND_PRECEDENCE;
    case TokenType::Like:
        [[fallthrough]];
    case TokenType::In:
        [[fallthrough]];
    case TokenType::Between:
        [[fallthrough]];
    case TokenType::Equal:
        [[fallthrough]];
    case TokenType::NotEqual:
        [[fallthrough]];
    case TokenType::LessThan:
        [[fallthrough]];
    case TokenType::LessEqual:
        [[fallthrough]];
    case TokenType::GreaterThan:
        [[fallthrough]];
    case TokenType::GreaterEqual:
        return COMPARISON_PRECEDENCE;
    case TokenType::Plus:
        [[fallthrough]];
    case TokenType::Minus:
        return ADDITIVE_PRECEDENCE;
    case TokenType::Star:
        [[fallthrough]];
    case TokenType::Slash:
        [[fallthrough]];
    case TokenType::Modulo:
        return MULTIPLICATIVE_PRECEDENCE;
    default:
        return std::nullopt;
    }
}

bool is_not_comparison(TokenType type) noexcept
{
    return type == TokenType::Like || type == TokenType::In || type == TokenType::Between;
}

} // namespace

ParserExpressionWorker::ParserExpressionWorker(ParserContext & context) noexcept
    : context_(context)
{}

std::expected<ParsedExpression, ParserError> ParserExpressionWorker::parse_expression()
{
    return parse_expression_precedence(OR_PRECEDENCE, true);
}

std::expected<ParsedExpression, ParserError> ParserExpressionWorker::parse_nested_expression(
    TokenLocation location
)
{
    auto guard = context_.enter_expression_nesting(location);
    if (!guard.has_value()) [[unlikely]] {
        return std::unexpected(std::move(guard.error()));
    }

    return parse_expression_precedence(OR_PRECEDENCE, true);
}

std::expected<ParsedExpression, ParserError> ParserExpressionWorker::parse_expression_precedence(
    int minimum_precedence,
    bool allow_not_prefix,
    bool * comparison_consumed_out
)
{
    std::vector<Token> not_operators;
    if (allow_not_prefix) {
        while (context_.check(TokenType::Not)) {
            const Token op = context_.advance();
            not_operators.push_back(op);

            if (not_operators.size() > MaxExpressionDepth) [[unlikely]] {
                auto depth =
                    context_.make_expression_parent_depth(MaxExpressionDepth, op.location());
                return std::unexpected(std::move(depth.error()));
            }
        }
    }

    std::expected<ParsedExpression, ParserError> left;
    bool comparison_consumed = false;
    if (!not_operators.empty()) {
        bool operand_comparison_consumed = false;
        auto operand =
            parse_expression_precedence(COMPARISON_PRECEDENCE, false, &operand_comparison_consumed);
        if (!operand.has_value()) [[unlikely]] {
            return std::unexpected(std::move(operand.error()));
        }

        for (auto it = not_operators.rbegin(); it != not_operators.rend(); ++it) {
            auto wrapped = make_unary(std::move(*operand), it->type(), it->location());
            if (!wrapped.has_value()) [[unlikely]] {
                return std::unexpected(std::move(wrapped.error()));
            }
            operand = std::move(wrapped);
        }
        left = std::move(operand);
        comparison_consumed = operand_comparison_consumed;
    } else {
        std::vector<Token> unary_operators;
        while (context_.check(TokenType::Plus) || context_.check(TokenType::Minus)) {
            const Token op = context_.advance();
            unary_operators.push_back(op);

            if (unary_operators.size() > MaxExpressionDepth) [[unlikely]] {
                auto depth =
                    context_.make_expression_parent_depth(MaxExpressionDepth, op.location());
                return std::unexpected(std::move(depth.error()));
            }
        }

        auto operand = parse_primary_expression();
        if (!operand.has_value()) [[unlikely]] {
            return std::unexpected(std::move(operand.error()));
        }

        for (auto it = unary_operators.rbegin(); it != unary_operators.rend(); ++it) {
            auto wrapped = make_unary(std::move(*operand), it->type(), it->location());
            if (!wrapped.has_value()) [[unlikely]] {
                return std::unexpected(std::move(wrapped.error()));
            }
            operand = std::move(wrapped);
        }
        left = std::move(operand);
    }

    while (true) {
        bool negated = false;
        TokenLocation not_location = context_.current().location();

        if (context_.check(TokenType::Not)) {
            if (comparison_consumed) {
                break;
            }
            if (!is_not_comparison(context_.peek_next().type())) {
                return std::unexpected(context_.make_current_error(
                    ParserErrorCode::ExpectedToken,
                    "Expected LIKE, IN, or BETWEEN after NOT"
                ));
            }
            negated = true;
            not_location = context_.advance().location();
        }

        const auto precedence = infix_precedence(context_.current().type());
        if (!precedence.has_value() || *precedence < minimum_precedence ||
            (comparison_consumed && *precedence == COMPARISON_PRECEDENCE)) {
            break;
        }

        const Token op = context_.advance();

        if (is_comparison_operator(op.type())) {
            comparison_consumed = true;
            auto right = parse_expression_precedence(ADDITIVE_PRECEDENCE, false);
            if (!right.has_value()) [[unlikely]] {
                return std::unexpected(std::move(right.error()));
            }

            auto combined =
                make_binary(std::move(*left), op.type(), std::move(*right), op.location());
            if (!combined.has_value()) [[unlikely]] {
                return std::unexpected(std::move(combined.error()));
            }
            left = std::move(combined);
            continue;
        }

        if (op.type() == TokenType::Like) {
            comparison_consumed = true;
            auto pattern = parse_expression_precedence(ADDITIVE_PRECEDENCE, false);
            if (!pattern.has_value()) [[unlikely]] {
                return std::unexpected(std::move(pattern.error()));
            }

            auto depth = context_.make_expression_parent_depth(
                max_depth(left->depth, pattern->depth),
                op.location()
            );
            if (!depth.has_value()) [[unlikely]] {
                return std::unexpected(std::move(depth.error()));
            }

            ParsedExpression expression {
                std::make_unique<ast::LikeExpression>(
                    std::move(left->expression),
                    std::move(pattern->expression),
                    context_.ast_location(op.location())
                ),
                *depth,
            };
            if (negated) {
                auto wrapped = make_unary(std::move(expression), TokenType::Not, not_location);
                if (!wrapped.has_value()) [[unlikely]] {
                    return std::unexpected(std::move(wrapped.error()));
                }
                left = std::move(wrapped);
            } else {
                left = std::move(expression);
            }
            continue;
        }

        if (op.type() == TokenType::In) {
            comparison_consumed = true;
            auto left_paren = context_.consume(TokenType::LeftParen, "Expected '(' after IN");
            if (!left_paren.has_value()) [[unlikely]] {
                return std::unexpected(std::move(left_paren.error()));
            }
            if (context_.check(TokenType::RightParen)) [[unlikely]] {
                return std::unexpected(context_.make_current_error(
                    ParserErrorCode::EmptyList,
                    "Expected at least one IN value"
                ));
            }

            std::vector<std::unique_ptr<ast::ExpressionNode>> values;
            std::size_t values_depth = 0;
            while (true) {
                const auto nested_location = left_paren->location();
                if (context_.expression_nesting_limit_reached()) [[unlikely]] {
                    return std::unexpected(context_.make_expression_nesting_error(nested_location));
                }

                auto value = parse_nested_expression(nested_location);
                if (!value.has_value()) [[unlikely]] {
                    return std::unexpected(std::move(value.error()));
                }
                values_depth = max_depth(values_depth, value->depth);
                values.push_back(std::move(value->expression));

                if (!context_.match(TokenType::Comma)) {
                    break;
                }
            }

            auto right_paren =
                context_.consume(TokenType::RightParen, "Expected ')' after IN values");
            if (!right_paren.has_value()) [[unlikely]] {
                return std::unexpected(std::move(right_paren.error()));
            }

            auto depth = context_.make_expression_parent_depth(
                max_depth(left->depth, values_depth),
                left_paren->location()
            );
            if (!depth.has_value()) [[unlikely]] {
                return std::unexpected(std::move(depth.error()));
            }

            ParsedExpression expression {
                std::make_unique<ast::InExpression>(
                    std::move(left->expression),
                    std::move(values),
                    context_.ast_location(op.location())
                ),
                *depth,
            };
            if (negated) {
                auto wrapped = make_unary(std::move(expression), TokenType::Not, not_location);
                if (!wrapped.has_value()) [[unlikely]] {
                    return std::unexpected(std::move(wrapped.error()));
                }
                left = std::move(wrapped);
            } else {
                left = std::move(expression);
            }
            continue;
        }

        if (op.type() == TokenType::Between) {
            comparison_consumed = true;
            auto lower = parse_expression_precedence(ADDITIVE_PRECEDENCE, false);
            if (!lower.has_value()) [[unlikely]] {
                return std::unexpected(std::move(lower.error()));
            }

            auto and_token = context_.consume(TokenType::And, "Expected AND in BETWEEN expression");
            if (!and_token.has_value()) [[unlikely]] {
                return std::unexpected(std::move(and_token.error()));
            }

            auto upper = parse_expression_precedence(ADDITIVE_PRECEDENCE, false);
            if (!upper.has_value()) [[unlikely]] {
                return std::unexpected(std::move(upper.error()));
            }

            auto depth = context_.make_expression_parent_depth(
                max_depth(left->depth, max_depth(lower->depth, upper->depth)),
                op.location()
            );
            if (!depth.has_value()) [[unlikely]] {
                return std::unexpected(std::move(depth.error()));
            }

            ParsedExpression expression {
                std::make_unique<ast::BetweenExpression>(
                    std::move(left->expression),
                    std::move(lower->expression),
                    std::move(upper->expression),
                    context_.ast_location(op.location())
                ),
                *depth,
            };
            if (negated) {
                auto wrapped = make_unary(std::move(expression), TokenType::Not, not_location);
                if (!wrapped.has_value()) [[unlikely]] {
                    return std::unexpected(std::move(wrapped.error()));
                }
                left = std::move(wrapped);
            } else {
                left = std::move(expression);
            }
            continue;
        }

        auto right = parse_expression_precedence(*precedence + 1, *precedence <= AND_PRECEDENCE);
        if (!right.has_value()) [[unlikely]] {
            return std::unexpected(std::move(right.error()));
        }

        auto combined = make_binary(std::move(*left), op.type(), std::move(*right), op.location());
        if (!combined.has_value()) [[unlikely]] {
            return std::unexpected(std::move(combined.error()));
        }
        left = std::move(combined);
    }

    if (comparison_consumed_out != nullptr) {
        *comparison_consumed_out = comparison_consumed;
    }
    return left;
}

std::expected<ParsedExpression, ParserError> ParserExpressionWorker::parse_primary_expression()
{
    if (is_literal_token(context_.current().type())) {
        return parse_literal_expression();
    }
    if (context_.check(TokenType::Identifier)) {
        if (context_.peek_next().type() == TokenType::LeftParen) {
            if (context_.expression_nesting_limit_reached() &&
                context_.peek_after_next().type() != TokenType::RightParen) [[unlikely]] {
                return std::unexpected(
                    context_.make_expression_nesting_error(context_.peek_next().location())
                );
            }
            return parse_function_call_expression();
        }
        return parse_column_reference_expression();
    }
    if (context_.check(TokenType::LeftBracket)) {
        if (context_.expression_nesting_limit_reached()) [[unlikely]] {
            return std::unexpected(
                context_.make_expression_nesting_error(context_.current().location())
            );
        }
        return parse_vector_expression();
    }
    if (context_.check(TokenType::LeftParen)) {
        const Token left_paren = context_.advance();
        if (context_.expression_nesting_limit_reached()) [[unlikely]] {
            return std::unexpected(context_.make_expression_nesting_error(left_paren.location()));
        }

        auto guard = context_.enter_expression_nesting(left_paren.location());
        if (!guard.has_value()) [[unlikely]] {
            return std::unexpected(std::move(guard.error()));
        }

        auto expression = parse_expression_precedence(OR_PRECEDENCE, true);
        if (!expression.has_value()) [[unlikely]] {
            return std::unexpected(std::move(expression.error()));
        }

        auto right_paren = context_.consume(TokenType::RightParen, "Expected ')' after expression");
        if (!right_paren.has_value()) [[unlikely]] {
            return std::unexpected(std::move(right_paren.error()));
        }

        return expression;
    }

    return std::unexpected(
        context_.make_current_error(ParserErrorCode::ExpectedExpression, "Expected expression")
    );
}

std::expected<ParsedExpression, ParserError>
ParserExpressionWorker::parse_column_reference_expression()
{
    auto first = context_.consume(TokenType::Identifier, "Expected column name");
    if (!first.has_value()) [[unlikely]] {
        return std::unexpected(std::move(first.error()));
    }

    std::optional<std::string> qualifier;
    std::string column(first->value());

    if (context_.match(TokenType::Dot)) {
        qualifier = std::move(column);

        auto second = context_.consume(TokenType::Identifier, "Expected column name after '.'");
        if (!second.has_value()) [[unlikely]] {
            return std::unexpected(std::move(second.error()));
        }
        column = std::string(second->value());
    }

    return ParsedExpression {
        std::make_unique<ast::ColumnReferenceExpression>(
            std::move(qualifier),
            std::move(column),
            context_.ast_location(first->location())
        ),
        1,
    };
}

std::expected<ParsedExpression, ParserError>
ParserExpressionWorker::parse_function_call_expression()
{
    auto name = context_.consume(TokenType::Identifier, "Expected function name");
    if (!name.has_value()) [[unlikely]] {
        return std::unexpected(std::move(name.error()));
    }

    auto left_paren = context_.consume(TokenType::LeftParen, "Expected '(' after function name");
    if (!left_paren.has_value()) [[unlikely]] {
        return std::unexpected(std::move(left_paren.error()));
    }

    std::vector<std::unique_ptr<ast::ExpressionNode>> arguments;
    std::size_t arguments_depth = 0;
    if (!context_.check(TokenType::RightParen)) {
        while (true) {
            if (context_.expression_nesting_limit_reached()) [[unlikely]] {
                return std::unexpected(
                    context_.make_expression_nesting_error(left_paren->location())
                );
            }

            auto argument = parse_nested_expression(left_paren->location());
            if (!argument.has_value()) [[unlikely]] {
                return std::unexpected(std::move(argument.error()));
            }
            arguments_depth = max_depth(arguments_depth, argument->depth);
            arguments.push_back(std::move(argument->expression));
            if (!context_.match(TokenType::Comma)) {
                break;
            }
        }
    }

    auto right_paren =
        context_.consume(TokenType::RightParen, "Expected ')' after function arguments");
    if (!right_paren.has_value()) [[unlikely]] {
        return std::unexpected(std::move(right_paren.error()));
    }

    auto depth = context_.make_expression_parent_depth(arguments_depth, left_paren->location());
    if (!depth.has_value()) [[unlikely]] {
        return std::unexpected(std::move(depth.error()));
    }

    return ParsedExpression {
        std::make_unique<ast::FunctionCallExpression>(
            std::string(name->value()),
            std::move(arguments),
            context_.ast_location(name->location())
        ),
        *depth,
    };
}

std::expected<ParsedExpression, ParserError> ParserExpressionWorker::parse_literal_expression()
{
    if (context_.check(TokenType::LeftBracket)) {
        return parse_vector_expression();
    }

    if (!is_literal_token(context_.current().type())) [[unlikely]] {
        return std::unexpected(
            context_.make_current_error(ParserErrorCode::ExpectedLiteral, "Expected literal")
        );
    }

    const Token token = context_.advance();
    return ParsedExpression {
        std::make_unique<ast::LiteralExpression>(
            token.type(),
            std::string(token.value()),
            context_.ast_location(token.location())
        ),
        1,
    };
}

std::expected<ParsedExpression, ParserError> ParserExpressionWorker::parse_vector_expression()
{
    const Token left_bracket = context_.advance();
    if (context_.check(TokenType::RightBracket)) [[unlikely]] {
        return std::unexpected(context_.make_current_error(
            ParserErrorCode::EmptyList,
            "Expected at least one vector element"
        ));
    }

    std::vector<std::unique_ptr<ast::ExpressionNode>> elements;
    std::size_t elements_depth = 0;
    while (true) {
        if (context_.expression_nesting_limit_reached()) [[unlikely]] {
            return std::unexpected(context_.make_expression_nesting_error(left_bracket.location()));
        }

        auto element = parse_nested_expression(left_bracket.location());
        if (!element.has_value()) [[unlikely]] {
            return std::unexpected(std::move(element.error()));
        }
        elements_depth = max_depth(elements_depth, element->depth);
        elements.push_back(std::move(element->expression));

        if (!context_.match(TokenType::Comma)) {
            break;
        }
    }

    auto right_bracket =
        context_.consume(TokenType::RightBracket, "Expected ']' after vector literal");
    if (!right_bracket.has_value()) [[unlikely]] {
        return std::unexpected(std::move(right_bracket.error()));
    }

    auto depth = context_.make_expression_parent_depth(elements_depth, left_bracket.location());
    if (!depth.has_value()) [[unlikely]] {
        return std::unexpected(std::move(depth.error()));
    }

    return ParsedExpression {
        std::make_unique<ast::VectorExpression>(
            std::move(elements),
            context_.ast_location(left_bracket.location())
        ),
        *depth,
    };
}

std::expected<ParsedExpression, ParserError> ParserExpressionWorker::make_unary(
    ParsedExpression operand,
    TokenType op,
    TokenLocation location
) const
{
    auto depth = context_.make_expression_parent_depth(operand.depth, location);
    if (!depth.has_value()) [[unlikely]] {
        return std::unexpected(std::move(depth.error()));
    }

    return ParsedExpression {
        std::make_unique<ast::UnaryExpression>(
            op,
            std::move(operand.expression),
            context_.ast_location(location)
        ),
        *depth,
    };
}

std::expected<ParsedExpression, ParserError> ParserExpressionWorker::make_binary(
    ParsedExpression left,
    TokenType op,
    ParsedExpression right,
    TokenLocation location
) const
{
    auto depth =
        context_.make_expression_parent_depth(max_depth(left.depth, right.depth), location);
    if (!depth.has_value()) [[unlikely]] {
        return std::unexpected(std::move(depth.error()));
    }

    return ParsedExpression {
        std::make_unique<ast::BinaryExpression>(
            std::move(left.expression),
            op,
            std::move(right.expression),
            context_.ast_location(location)
        ),
        *depth,
    };
}

} // namespace litedb::core::parser

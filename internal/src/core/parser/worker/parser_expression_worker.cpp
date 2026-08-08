#include "core/parser/worker/parser_expression_worker.hpp"

#include <utility>

#include "core/parser/token.hpp"
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

namespace litedb::core::parser
{

ParserExpressionWorker::ParserExpressionWorker(ParserContext & context) noexcept
    : context_(context)
{
}

std::expected<std::unique_ptr<ast::ExpressionNode>, ParserError>
ParserExpressionWorker::parse_expression()
{
    return parse_or_expression();
}

std::expected<std::unique_ptr<ast::ExpressionNode>, ParserError>
ParserExpressionWorker::parse_or_expression()
{
    auto left = parse_and_expression();
    if (!left.has_value()) [[unlikely]] {
        return std::unexpected(std::move(left.error()));
    }

    while (context_.check(TokenType::Or)) {
        const Token op = context_.advance();

        auto right = parse_and_expression();
        if (!right.has_value()) [[unlikely]] {
            return std::unexpected(std::move(right.error()));
        }

        left = std::make_unique<ast::BinaryExpression>(
            std::move(*left),
            op.type(),
            std::move(*right),
            context_.ast_location(op.location())
        );
    }

    return left;
}

std::expected<std::unique_ptr<ast::ExpressionNode>, ParserError>
ParserExpressionWorker::parse_and_expression()
{
    auto left = parse_not_expression();
    if (!left.has_value()) [[unlikely]] {
        return std::unexpected(std::move(left.error()));
    }

    while (context_.check(TokenType::And)) {
        const Token op = context_.advance();

        auto right = parse_not_expression();
        if (!right.has_value()) [[unlikely]] {
            return std::unexpected(std::move(right.error()));
        }

        left = std::make_unique<ast::BinaryExpression>(
            std::move(*left),
            op.type(),
            std::move(*right),
            context_.ast_location(op.location())
        );
    }

    return left;
}

std::expected<std::unique_ptr<ast::ExpressionNode>, ParserError>
ParserExpressionWorker::parse_not_expression()
{
    if (context_.check(TokenType::Not)) {
        const Token op = context_.advance();

        auto operand = parse_not_expression();
        if (!operand.has_value()) [[unlikely]] {
            return std::unexpected(std::move(operand.error()));
        }

        return std::make_unique<ast::UnaryExpression>(
            op.type(),
            std::move(*operand),
            context_.ast_location(op.location())
        );
    }

    return parse_comparison_expression();
}

std::expected<std::unique_ptr<ast::ExpressionNode>, ParserError>
ParserExpressionWorker::parse_comparison_expression()
{
    auto left = parse_additive_expression();
    if (!left.has_value()) [[unlikely]] {
        return std::unexpected(std::move(left.error()));
    }

    if (is_comparison_operator(context_.current().type())) {
        const Token op = context_.advance();

        auto right = parse_additive_expression();
        if (!right.has_value()) [[unlikely]] {
            return std::unexpected(std::move(right.error()));
        }

        return std::make_unique<ast::BinaryExpression>(
            std::move(*left),
            op.type(),
            std::move(*right),
            context_.ast_location(op.location())
        );
    }

    bool negated = false;
    TokenLocation not_location = context_.current().location();
    if (context_.check(TokenType::Not)) {
        negated = true;
        not_location = context_.advance().location();
    }

    if (context_.check(TokenType::Like)) {
        const Token op = context_.advance();

        auto pattern = parse_additive_expression();
        if (!pattern.has_value()) [[unlikely]] {
            return std::unexpected(std::move(pattern.error()));
        }

        std::unique_ptr<ast::ExpressionNode> expression = std::make_unique<ast::LikeExpression>(
            std::move(*left),
            std::move(*pattern),
            context_.ast_location(op.location())
        );
        if (negated) {
            expression = std::make_unique<ast::UnaryExpression>(
                TokenType::Not,
                std::move(expression),
                context_.ast_location(not_location)
            );
        }
        return expression;
    }

    if (context_.check(TokenType::In)) {
        const Token op = context_.advance();

        auto left_paren = context_.consume(
            TokenType::LeftParen, "Expected '(' after IN"
        );
        if (!left_paren.has_value()) [[unlikely]] {
            return std::unexpected(std::move(left_paren.error()));
        }
        if (context_.check(TokenType::RightParen)) [[unlikely]] {
            return std::unexpected(context_.make_current_error(
                ParserErrorCode::EmptyList, "Expected at least one IN value"
            ));
        }

        std::vector<std::unique_ptr<ast::ExpressionNode>> values;
        while (true) {
            auto value = parse_expression();
            if (!value.has_value()) [[unlikely]] {
                return std::unexpected(std::move(value.error()));
            }
            values.push_back(std::move(*value));

            if (!context_.match(TokenType::Comma)) {
                break;
            }
        }

        auto right_paren = context_.consume(
            TokenType::RightParen, "Expected ')' after IN values"
        );
        if (!right_paren.has_value()) [[unlikely]] {
            return std::unexpected(std::move(right_paren.error()));
        }

        std::unique_ptr<ast::ExpressionNode> expression = std::make_unique<ast::InExpression>(
            std::move(*left),
            std::move(values),
            context_.ast_location(op.location())
        );
        if (negated) {
            expression = std::make_unique<ast::UnaryExpression>(
                TokenType::Not,
                std::move(expression),
                context_.ast_location(not_location)
            );
        }
        return expression;
    }

    if (context_.check(TokenType::Between)) {
        const Token op = context_.advance();

        auto lower = parse_additive_expression();
        if (!lower.has_value()) [[unlikely]] {
            return std::unexpected(std::move(lower.error()));
        }

        auto and_token = context_.consume(
            TokenType::And, "Expected AND in BETWEEN expression"
        );
        if (!and_token.has_value()) [[unlikely]] {
            return std::unexpected(std::move(and_token.error()));
        }

        auto upper = parse_additive_expression();
        if (!upper.has_value()) [[unlikely]] {
            return std::unexpected(std::move(upper.error()));
        }

        std::unique_ptr<ast::ExpressionNode> expression = std::make_unique<ast::BetweenExpression>(
            std::move(*left),
            std::move(*lower),
            std::move(*upper),
            context_.ast_location(op.location())
        );
        if (negated) {
            expression = std::make_unique<ast::UnaryExpression>(
                TokenType::Not,
                std::move(expression),
                context_.ast_location(not_location)
            );
        }
        return expression;
    }

    if (negated) [[unlikely]] {
        return std::unexpected(context_.make_current_error(
            ParserErrorCode::ExpectedToken, "Expected LIKE, IN, or BETWEEN after NOT"
        ));
    }

    return left;
}

std::expected<std::unique_ptr<ast::ExpressionNode>, ParserError>
ParserExpressionWorker::parse_additive_expression()
{
    auto left = parse_multiplicative_expression();
    if (!left.has_value()) [[unlikely]] {
        return std::unexpected(std::move(left.error()));
    }

    while (context_.check(TokenType::Plus) || context_.check(TokenType::Minus)) {
        const Token op = context_.advance();

        auto right = parse_multiplicative_expression();
        if (!right.has_value()) [[unlikely]] {
            return std::unexpected(std::move(right.error()));
        }

        left = std::make_unique<ast::BinaryExpression>(
            std::move(*left),
            op.type(),
            std::move(*right),
            context_.ast_location(op.location())
        );
    }

    return left;
}

std::expected<std::unique_ptr<ast::ExpressionNode>, ParserError>
ParserExpressionWorker::parse_multiplicative_expression()
{
    auto left = parse_unary_expression();
    if (!left.has_value()) [[unlikely]] {
        return std::unexpected(std::move(left.error()));
    }

    while (context_.check(TokenType::Star) || context_.check(TokenType::Slash) || context_.check(TokenType::Modulo)) {
        const Token op = context_.advance();

        auto right = parse_unary_expression();
        if (!right.has_value()) [[unlikely]] {
            return std::unexpected(std::move(right.error()));
        }

        left = std::make_unique<ast::BinaryExpression>(
            std::move(*left),
            op.type(),
            std::move(*right),
            context_.ast_location(op.location())
        );
    }

    return left;
}

std::expected<std::unique_ptr<ast::ExpressionNode>, ParserError>
ParserExpressionWorker::parse_unary_expression()
{
    if (context_.check(TokenType::Plus) || context_.check(TokenType::Minus)) {
        const Token op = context_.advance();

        auto operand = parse_unary_expression();
        if (!operand.has_value()) [[unlikely]] {
            return std::unexpected(std::move(operand.error()));
        }

        return std::make_unique<ast::UnaryExpression>(
            op.type(),
            std::move(*operand),
            context_.ast_location(op.location())
        );
    }

    return parse_primary_expression();
}

std::expected<std::unique_ptr<ast::ExpressionNode>, ParserError>
ParserExpressionWorker::parse_primary_expression()
{
    if (is_literal_token(context_.current().type())) {
        return parse_literal_expression();
    }
    if (context_.check(TokenType::Identifier)) {
        if (context_.peek_next().type() == TokenType::LeftParen) {
            return parse_function_call_expression();
        }
        return parse_column_reference_expression();
    }
    if (context_.check(TokenType::LeftBracket)) {
        return parse_vector_expression();
    }
    if (context_.match(TokenType::LeftParen)) {
        auto expression = parse_expression();
        if (!expression.has_value()) [[unlikely]] {
            return std::unexpected(std::move(expression.error()));
        }

        auto right_paren = context_.consume(
            TokenType::RightParen, "Expected ')' after expression"
        );
        if (!right_paren.has_value()) [[unlikely]] {
            return std::unexpected(std::move(right_paren.error()));
        }

        return expression;
    }

    return std::unexpected(context_.make_current_error(
        ParserErrorCode::ExpectedExpression, "Expected expression"
    ));
}

std::expected<std::unique_ptr<ast::ExpressionNode>, ParserError>
ParserExpressionWorker::parse_column_reference_expression()
{
    auto first = context_.consume(
        TokenType::Identifier, "Expected column name"
    );
    if (!first.has_value()) [[unlikely]] {
        return std::unexpected(std::move(first.error()));
    }

    std::optional<std::string> qualifier;
    std::string column(first->value());

    if (context_.match(TokenType::Dot)) {
        qualifier = std::move(column);

        auto second = context_.consume(
            TokenType::Identifier, "Expected column name after '.'"
        );
        if (!second.has_value()) [[unlikely]] {
            return std::unexpected(std::move(second.error()));
        }
        column = std::string(second->value());
    }

    return std::make_unique<ast::ColumnReferenceExpression>(
        std::move(qualifier),
        std::move(column),
        context_.ast_location(first->location())
    );
}

std::expected<std::unique_ptr<ast::ExpressionNode>, ParserError>
ParserExpressionWorker::parse_function_call_expression()
{
    auto name = context_.consume(
        TokenType::Identifier, "Expected function name"
    );
    if (!name.has_value()) [[unlikely]] {
        return std::unexpected(std::move(name.error()));
    }

    auto left_paren = context_.consume(
        TokenType::LeftParen, "Expected '(' after function name"
    );
    if (!left_paren.has_value()) [[unlikely]] {
        return std::unexpected(std::move(left_paren.error()));
    }

    std::vector<std::unique_ptr<ast::ExpressionNode>> arguments;
    if (!context_.check(TokenType::RightParen)) {
        while (true) {
            auto argument = parse_expression();
            if (!argument.has_value()) [[unlikely]] {
                return std::unexpected(std::move(argument.error()));
            }
            arguments.push_back(std::move(*argument));
            if (!context_.match(TokenType::Comma)) {
                break;
            }
        }
    }

    auto right_paren = context_.consume(
        TokenType::RightParen, "Expected ')' after function arguments"
    );
    if (!right_paren.has_value()) [[unlikely]] {
        return std::unexpected(std::move(right_paren.error()));
    }

    return std::make_unique<ast::FunctionCallExpression>(
        std::string(name->value()),
        std::move(arguments),
        context_.ast_location(name->location())
    );
}

std::expected<std::unique_ptr<ast::ExpressionNode>, ParserError>
ParserExpressionWorker::parse_literal_expression()
{
    if (context_.check(TokenType::LeftBracket)) {
        return parse_vector_expression();
    }

    if (!is_literal_token(context_.current().type())) [[unlikely]] {
        return std::unexpected(context_.make_current_error(
            ParserErrorCode::ExpectedLiteral, "Expected literal"
        ));
    }

    const Token token = context_.advance();
    return std::make_unique<ast::LiteralExpression>(
        token.type(),
        std::string(token.value()),
        context_.ast_location(token.location())
    );
}

std::expected<std::unique_ptr<ast::ExpressionNode>, ParserError>
ParserExpressionWorker::parse_vector_expression()
{
    const Token left_bracket = context_.advance();
    if (context_.check(TokenType::RightBracket)) [[unlikely]] {
        return std::unexpected(context_.make_current_error(
            ParserErrorCode::EmptyList, "Expected at least one vector element"
        ));
    }

    std::vector<std::unique_ptr<ast::ExpressionNode>> elements;
    while (true) {
        auto element = parse_expression();
        if (!element.has_value()) [[unlikely]] {
            return std::unexpected(std::move(element.error()));
        }
        elements.push_back(std::move(*element));

        if (!context_.match(TokenType::Comma)) {
            break;
        }
    }

    auto right_bracket = context_.consume(
        TokenType::RightBracket, "Expected ']' after vector literal"
    );
    if (!right_bracket.has_value()) [[unlikely]] {
        return std::unexpected(std::move(right_bracket.error()));
    }

    return std::make_unique<ast::VectorExpression>(
        std::move(elements),
        context_.ast_location(left_bracket.location())
    );
}

} // namespace litedb::core::parser

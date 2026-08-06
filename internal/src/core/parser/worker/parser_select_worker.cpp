#include "core/parser/worker/parser_select_worker.hpp"

#include <cstddef>
#include <optional>
#include <string>
#include <utility>

#include "core/parser/ast/expression/alias_expression.hpp"
#include "core/parser/ast/expression/wildcard_expression.hpp"
#include "core/parser/ast/statement/select_statement.hpp"
#include "core/parser/worker/parser_expression_worker.hpp"
#include "core/parser/worker/parser_schema_helper.hpp"

namespace litedb::core::parser
{

ParserSelectWorker::ParserSelectWorker(ParserContext & context)
    : context_(context)
{
}

std::expected<std::unique_ptr<ast::StatementNode>, ParserError>
ParserSelectWorker::parse_select_statement()
{
    const TokenLocation location = context_.current().location();
    context_.advance();

    ParserExpressionWorker expression_worker(context_);
    ParserSchemaHelper schema_worker(context_);

    ast::SelectStatement::SelectList select_list;
    while (true) {
        auto item = parse_select_item();
        if (!item.has_value()) [[unlikely]] {
            return std::unexpected(std::move(item.error()));
        }
        select_list.push_back(std::move(*item));

        if (!context_.match(TokenType::Comma)) {
            break;
        }
    }

    auto from = context_.consume(
        TokenType::From, "Expected FROM after select list"
    );
    if (!from.has_value()) [[unlikely]] {
        return std::unexpected(std::move(from.error()));
    }

    auto collection = schema_worker.parse_identifier_string(
        "Expected collection name"
    );
    if (!collection.has_value()) [[unlikely]] {
        return std::unexpected(std::move(collection.error()));
    }

    std::unique_ptr<ast::ExpressionNode> where;
    if (context_.match(TokenType::Where)) {
        auto expression = expression_worker.parse_expression();
        if (!expression.has_value()) [[unlikely]] {
            return std::unexpected(std::move(expression.error()));
        }
        where = std::move(*expression);
    }

    ast::SelectStatement::OrderByList order_by;
    if (context_.match(TokenType::Order)) {
        auto by = context_.consume(
            TokenType::By, "Expected BY after ORDER"
        );
        if (!by.has_value()) [[unlikely]] {
            return std::unexpected(std::move(by.error()));
        }

        while (true) {
            auto expression = expression_worker.parse_expression();
            if (!expression.has_value()) [[unlikely]] {
                return std::unexpected(std::move(expression.error()));
            }

            bool ascending = true;
            if (context_.match(TokenType::Asc)) {
                ascending = true;
            } else if (context_.match(TokenType::Desc)) {
                ascending = false;
            }

            order_by.push_back(ast::OrderByItem {std::move(*expression), ascending});

            if (!context_.match(TokenType::Comma)) {
                break;
            }
        }
    }

    std::optional<std::size_t> limit;
    if (context_.match(TokenType::Limit)) {
        auto value = schema_worker.parse_integer_value(
            "Expected LIMIT value"
        );
        if (!value.has_value()) [[unlikely]] {
            return std::unexpected(std::move(value.error()));
        }
        limit = *value;
    }

    std::optional<std::size_t> offset;
    if (context_.match(TokenType::Offset)) {
        auto value = schema_worker.parse_integer_value(
            "Expected OFFSET value"
        );
        if (!value.has_value()) [[unlikely]] {
            return std::unexpected(std::move(value.error()));
        }
        offset = *value;
    }

    return std::make_unique<ast::SelectStatement>(
        std::move(select_list),
        std::move(*collection),
        std::move(where),
        std::move(order_by),
        limit,
        offset,
        context_.ast_location(location)
    );
}

std::expected<std::unique_ptr<ast::ExpressionNode>, ParserError>
ParserSelectWorker::parse_select_item()
{
    if (context_.check(TokenType::Star)) {
        const Token star = context_.advance();
        if (context_.check(TokenType::As)) [[unlikely]] {
            return std::unexpected(context_.make_current_error(
                ParserErrorCode::UnexpectedToken,
                "Wildcard select item cannot have alias"
            ));
        }
        return std::make_unique<ast::WildcardExpression>(context_.ast_location(star.location()));
    }

    if (context_.check(TokenType::Identifier)
        && context_.peek_next().type() == TokenType::Dot
        && context_.peek_after_next().type() == TokenType::Star) {
        const Token qualifier = context_.advance();
        context_.advance();
        context_.advance();
        if (context_.check(TokenType::As)) [[unlikely]] {
            return std::unexpected(context_.make_current_error(
                ParserErrorCode::UnexpectedToken,
                "Wildcard select item cannot have alias"
            ));
        }
        return std::make_unique<ast::WildcardExpression>(
            std::string(qualifier.value()),
            context_.ast_location(qualifier.location())
        );
    }

    ParserExpressionWorker expression_worker(context_);
    auto expression = expression_worker.parse_expression();
    if (!expression.has_value()) [[unlikely]] {
        return std::unexpected(std::move(expression.error()));
    }

    if (!context_.match(TokenType::As)) {
        return expression;
    }

    auto alias = context_.consume(
        TokenType::Identifier,
        "Expected alias after AS",
        ParserErrorCode::ExpectedIdentifier
    );
    if (!alias.has_value()) [[unlikely]] {
        return std::unexpected(std::move(alias.error()));
    }

    const auto location = (*expression)->location();
    return std::make_unique<ast::AliasExpression>(
        std::move(*expression),
        std::string(alias->value()),
        location
    );
}

} // namespace litedb::core::parser

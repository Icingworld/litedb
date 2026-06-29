#include "core/parser/worker/parser_insert_worker.hpp"

#include <expected>
#include <memory>
#include <utility>

#include "core/parser/ast/statement/insert_statement.hpp"
#include "core/parser/worker/parser_expression_worker.hpp"
#include "core/parser/worker/parser_schema_helper.hpp"

namespace litedb::core::parser
{

ParserInsertWorker::ParserInsertWorker(ParserContext & context)
    : context_(context)
{
}

std::expected<std::unique_ptr<ast::StatementNode>, ParserError> ParserInsertWorker::parse_insert_statement()
{
    const TokenLocation location = context_.current().location();
    context_.advance();

    auto into = context_.consume(TokenType::Into, "Expected INTO after INSERT");
    if (!into.has_value()) [[unlikely]] {
        return std::unexpected(into.error());
    }

    ParserSchemaHelper schema_helper(context_);
    auto collection = schema_helper.parse_identifier_string("Expected collection name");
    if (!collection.has_value()) [[unlikely]] {
        return std::unexpected(collection.error());
    }

    ast::InsertStatement::ColumnList columns;
    // 列名列表是可省略的
    if (context_.match(TokenType::LeftParen)) {
        if (context_.check(TokenType::RightParen)) [[unlikely]] {
            return std::unexpected(context_.make_current_error(ParserErrorCode::EmptyList, "Expected at least one column name"));
        }

        while (true) {
            auto column = schema_helper.parse_identifier_string("Expected column name");
            if (!column.has_value()) [[unlikely]] {
                return std::unexpected(column.error());
            }
            columns.push_back(std::move(column.value()));

            if (!context_.match(TokenType::Comma)) {
                break;
            }
        }

        auto right_paren = context_.consume(TokenType::RightParen, "Expected ')' after column list");
        if (!right_paren.has_value()) [[unlikely]] {
            return std::unexpected(right_paren.error());
        }
    }

    auto values = context_.consume(TokenType::Values, "Expected VALUES after INSERT target");
    if (!values.has_value()) [[unlikely]] {
        return std::unexpected(values.error());
    }
    auto left_paren = context_.consume(TokenType::LeftParen, "Expected '(' before values");
    if (!left_paren.has_value()) [[unlikely]] {
        return std::unexpected(left_paren.error());
    }
    if (context_.check(TokenType::RightParen)) [[unlikely]] {
        return std::unexpected(context_.make_current_error(ParserErrorCode::EmptyList, "Expected at least one value"));
    }

    ParserExpressionWorker expression_worker(context_);
    ast::InsertStatement::ValueList value_list;
    while (true) {
        auto value = expression_worker.parse_expression();
        if (!value.has_value()) [[unlikely]] {
            return std::unexpected(value.error());
        }
        value_list.push_back(std::move(value.value()));

        if (!context_.match(TokenType::Comma)) {
            break;
        }
    }

    auto right_paren = context_.consume(TokenType::RightParen, "Expected ')' after values");
    if (!right_paren.has_value()) [[unlikely]] {
        return std::unexpected(right_paren.error());
    }

    return std::make_unique<ast::InsertStatement>(
        std::move(collection.value()),
        std::move(columns),
        std::move(value_list),
        context_.ast_location(location)
    );
}

} // namespace litedb::core::parser

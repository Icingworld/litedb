#include "core/parser/parser_select_worker.hpp"

#include <expected>
#include <memory>
#include <utility>

#include "core/parser/ast/statement/select_statement.hpp"
#include "core/parser/parser_expression_worker.hpp"
#include "core/parser/parser_schema_worker.hpp"

namespace litedb::core::parser
{

ParserSelectWorker::ParserSelectWorker(ParserContext & context)
    : context_(context)
{
}

std::expected<std::unique_ptr<ast::StatementNode>, ParserError> ParserSelectWorker::parse_select_statement()
{
    const TokenLocation location = context_.current().location();
    context_.advance();

    ParserExpressionWorker expression_worker(context_);
    ParserSchemaWorker schema_worker(context_);

    ast::SelectStatement::SelectList select_list;
    while (true) {
        auto item = expression_worker.parse_wildcard_or_column_reference();
        if (!item.has_value()) [[unlikely]] {
            return std::unexpected(item.error());
        }
        select_list.push_back(std::move(item.value()));

        if (!context_.match(TokenType::Comma)) {
            break;
        }
    }

    auto from = context_.consume(TokenType::From, "Expected FROM after select list");
    if (!from.has_value()) [[unlikely]] {
        return std::unexpected(from.error());
    }

    auto collection = schema_worker.parse_identifier_string("Expected collection name");
    if (!collection.has_value()) [[unlikely]] {
        return std::unexpected(collection.error());
    }

    std::unique_ptr<ast::ExpressionNode> where;
    if (context_.match(TokenType::Where)) {
        auto expression = expression_worker.parse_expression();
        if (!expression.has_value()) [[unlikely]] {
            return std::unexpected(expression.error());
        }
        where = std::move(expression.value());
    }

    ast::SelectStatement::OrderByList order_by;
    if (context_.match(TokenType::Order)) {
        auto by = context_.consume(TokenType::By, "Expected BY after ORDER");
        if (!by.has_value()) [[unlikely]] {
            return std::unexpected(by.error());
        }

        while (true) {
            auto expression = expression_worker.parse_expression();
            if (!expression.has_value()) [[unlikely]] {
                return std::unexpected(expression.error());
            }

            bool ascending = true;
            if (context_.match(TokenType::Asc)) {
                ascending = true;
            } else if (context_.match(TokenType::Desc)) {
                ascending = false;
            }

            order_by.push_back(ast::OrderByItem {std::move(expression.value()), ascending});

            if (!context_.match(TokenType::Comma)) {
                break;
            }
        }
    }

    std::optional<std::size_t> limit;
    if (context_.match(TokenType::Limit)) {
        auto value = schema_worker.parse_integer_value("Expected LIMIT value");
        if (!value.has_value()) [[unlikely]] {
            return std::unexpected(value.error());
        }
        limit = value.value();
    }

    std::optional<std::size_t> offset;
    if (context_.match(TokenType::Offset)) {
        auto value = schema_worker.parse_integer_value("Expected OFFSET value");
        if (!value.has_value()) [[unlikely]] {
            return std::unexpected(value.error());
        }
        offset = value.value();
    }

    return std::make_unique<ast::SelectStatement>(
        std::move(select_list),
        std::move(collection.value()),
        std::move(where),
        std::move(order_by),
        limit,
        offset,
        context_.ast_location(location)
    );
}

} // namespace litedb::core::parser

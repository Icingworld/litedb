#include "core/parser/worker/parser_update_worker.hpp"

#include <expected>
#include <memory>
#include <utility>

#include "core/parser/ast/statement/update_statement.hpp"
#include "core/parser/worker/parser_expression_worker.hpp"
#include "core/parser/worker/parser_schema_helper.hpp"

namespace litedb::core::parser
{

ParserUpdateWorker::ParserUpdateWorker(ParserContext & context)
    : context_(context)
{
}

std::expected<std::unique_ptr<ast::StatementNode>, ParserError> ParserUpdateWorker::parse_update_statement()
{
    const TokenLocation location = context_.current().location();
    context_.advance();

    ParserSchemaHelper schema_helper(context_);
    auto collection = schema_helper.parse_identifier_string("Expected collection name");
    if (!collection.has_value()) [[unlikely]] {
        return std::unexpected(collection.error());
    }

    auto set = context_.consume(TokenType::Set, "Expected SET after collection name");
    if (!set.has_value()) [[unlikely]] {
        return std::unexpected(set.error());
    }

    ParserExpressionWorker expression_worker(context_);
    ast::UpdateStatement::AssignmentList assignments;
    while (true) {
        auto column = schema_helper.parse_identifier_string("Expected column name");
        if (!column.has_value()) [[unlikely]] {
            return std::unexpected(column.error());
        }

        auto equal = context_.consume(TokenType::Equal, "Expected '=' after column name");
        if (!equal.has_value()) {
            return std::unexpected(equal.error());
        }

        auto value = expression_worker.parse_expression();
        if (!value.has_value()) [[unlikely]] {
            return std::unexpected(value.error());
        }

        assignments.push_back(ast::Assignment {
            std::move(column.value()),
            std::move(value.value()),
        });

        if (!context_.match(TokenType::Comma)) {
            break;
        }
    }

    std::unique_ptr<ast::ExpressionNode> where;
    if (context_.match(TokenType::Where)) {
        auto expression = expression_worker.parse_expression();
        if (!expression.has_value()) [[unlikely]] {
            return std::unexpected(expression.error());
        }
        where = std::move(expression.value());
    }

    return std::make_unique<ast::UpdateStatement>(
        std::move(collection.value()),
        std::move(assignments),
        std::move(where),
        context_.ast_location(location)
    );
}

} // namespace litedb::core::parser

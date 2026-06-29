#include "core/parser/worker/parser_delete_worker.hpp"

#include <expected>
#include <memory>
#include <utility>

#include "core/parser/ast/statement/delete_statement.hpp"
#include "core/parser/worker/parser_expression_worker.hpp"
#include "core/parser/worker/parser_schema_helper.hpp"

namespace litedb::core::parser
{

ParserDeleteWorker::ParserDeleteWorker(ParserContext & context)
    : context_(context)
{
}

std::expected<std::unique_ptr<ast::StatementNode>, ParserError> ParserDeleteWorker::parse_delete_statement()
{
    const TokenLocation location = context_.current().location();
    context_.advance();

    auto from = context_.consume(TokenType::From, "Expected FROM after DELETE");
    if (!from.has_value()) [[unlikely]] {
        return std::unexpected(from.error());
    }

    ParserSchemaHelper schema_helper(context_);
    auto collection = schema_helper.parse_identifier_string("Expected collection name");
    if (!collection.has_value()) [[unlikely]] {
        return std::unexpected(collection.error());
    }

    std::unique_ptr<ast::ExpressionNode> where;
    if (context_.match(TokenType::Where)) {
        ParserExpressionWorker expression_worker(context_);
        auto expression = expression_worker.parse_expression();
        if (!expression.has_value()) [[unlikely]] {
            return std::unexpected(expression.error());
        }
        where = std::move(expression.value());
    }

    return std::make_unique<ast::DeleteStatement>(
        std::move(collection.value()),
        std::move(where),
        context_.ast_location(location)
    );
}

} // namespace litedb::core::parser

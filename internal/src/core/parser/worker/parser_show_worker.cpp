#include "core/parser/worker/parser_show_worker.hpp"

#include <expected>
#include <memory>

#include "core/parser/ast/statement/show_statement.hpp"

namespace litedb::core::parser
{

ParserShowWorker::ParserShowWorker(ParserContext & context)
    : context_(context)
{
}

std::expected<std::unique_ptr<ast::StatementNode>, ParserError> ParserShowWorker::parse_show_statement()
{
    const TokenLocation location = context_.current().location();
    context_.advance();

    if (context_.match(TokenType::Databases)) {
        return std::make_unique<ast::ShowStatement>(
            ast::SchemaObjectType::Database,
            context_.ast_location(location)
        );
    }

    if (context_.match(TokenType::Collections)) {
        return std::make_unique<ast::ShowStatement>(
            ast::SchemaObjectType::Collection,
            context_.ast_location(location)
        );
    }

    return std::unexpected(context_.make_current_error(
        ParserErrorCode::ExpectedToken,
        "Expected DATABASES or COLLECTIONS after SHOW"
    ));
}

} // namespace litedb::core::parser

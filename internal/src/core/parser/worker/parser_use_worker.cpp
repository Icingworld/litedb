#include "core/parser/worker/parser_use_worker.hpp"

#include <utility>

#include "core/parser/ast/statement/use_statement.hpp"
#include "core/parser/worker/parser_schema_helper.hpp"

namespace litedb::core::parser
{

ParserUseWorker::ParserUseWorker(ParserContext & context) noexcept
    : context_(context)
{
}

std::expected<std::unique_ptr<ast::StatementNode>, ParserError> ParserUseWorker::parse_use_statement()
{
    const TokenLocation location = context_.current().location();
    context_.advance();

    ParserSchemaHelper schema_helper(context_);
    auto database = schema_helper.parse_identifier_string(
        "Expected database name"
    );
    if (!database.has_value()) [[unlikely]] {
        return std::unexpected(std::move(database.error()));
    }

    return std::make_unique<ast::UseStatement>(
        std::move(*database),
        context_.ast_location(location)
    );
}

} // namespace litedb::core::parser

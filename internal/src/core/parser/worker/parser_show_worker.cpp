#include "core/parser/worker/parser_show_worker.hpp"

#include <expected>
#include <memory>

#include "core/parser/ast/statement/show_statement.hpp"
#include "core/parser/worker/parser_schema_helper.hpp"

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

    ParserSchemaHelper schema_helper(context_);
    auto object_type = schema_helper.parse_schema_object_type(true);
    if (!object_type.has_value()) [[unlikely]] {
        return std::unexpected(object_type.error());
    }

    return std::make_unique<ast::ShowStatement>(object_type.value(), context_.ast_location(location));
}

} // namespace litedb::core::parser

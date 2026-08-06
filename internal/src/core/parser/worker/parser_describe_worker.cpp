#include "core/parser/worker/parser_describe_worker.hpp"

#include <utility>

#include "core/parser/ast/statement/describe_collection_statement.hpp"
#include "core/parser/worker/parser_schema_helper.hpp"

namespace litedb::core::parser
{

ParserDescribeWorker::ParserDescribeWorker(ParserContext & context)
    : context_(context)
{
}

std::expected<std::unique_ptr<ast::StatementNode>, ParserError>
ParserDescribeWorker::parse_describe_collection_statement()
{
    const TokenLocation location = context_.current().location();
    context_.advance();

    // COLLECTION 关键字是可选的
    if (context_.check(TokenType::Collection)) {
        context_.advance();
    }

    ParserSchemaHelper schema_helper(context_);
    auto collection = schema_helper.parse_identifier_string(
        "Expected collection name"
    );
    if (!collection.has_value()) [[unlikely]] {
        return std::unexpected(std::move(collection.error()));
    }

    return std::make_unique<ast::DescribeCollectionStatement>(
        std::move(*collection),
        context_.ast_location(location)
    );
}

} // namespace litedb::core::parser

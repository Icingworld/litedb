#include "core/parser/worker/parser_show_worker.hpp"

#include <expected>
#include <memory>
#include <optional>
#include <string>
#include <utility>

#include "core/parser/ast/statement/show_collections_statement.hpp"
#include "core/parser/ast/statement/show_databases_statement.hpp"
#include "core/parser/ast/statement/show_indexes_statement.hpp"
#include "core/parser/ast/statement/show_vector_indexes_statement.hpp"

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
        return std::make_unique<ast::ShowDatabasesStatement>(
            context_.ast_location(location)
        );
    }

    if (context_.match(TokenType::Collections)) {
        std::optional<std::string> database_name;
        if (context_.match(TokenType::From)) {
            auto database = context_.consume(
                TokenType::Identifier,
                "Expected database name after FROM",
                ParserErrorCode::ExpectedIdentifier
            );
            if (!database.has_value()) [[unlikely]] {
                return std::unexpected(database.error());
            }
            database_name = std::string(database->value());
        }

        return std::make_unique<ast::ShowCollectionsStatement>(
            std::move(database_name),
            context_.ast_location(location)
        );
    }

    if (context_.match(TokenType::Indexes)) {
        auto from = context_.consume(TokenType::From, "Expected FROM after SHOW INDEXES");
        if (!from.has_value()) [[unlikely]] {
            return std::unexpected(from.error());
        }

        auto collection = context_.consume(
            TokenType::Identifier,
            "Expected collection name after FROM",
            ParserErrorCode::ExpectedIdentifier
        );
        if (!collection.has_value()) [[unlikely]] {
            return std::unexpected(collection.error());
        }

        return std::make_unique<ast::ShowIndexesStatement>(
            std::string(collection->value()),
            context_.ast_location(location)
        );
    }

    if (context_.match(TokenType::VIndexes)) {
        auto from = context_.consume(TokenType::From, "Expected FROM after SHOW VINDEXES");
        if (!from.has_value()) [[unlikely]] {
            return std::unexpected(from.error());
        }

        auto collection = context_.consume(
            TokenType::Identifier,
            "Expected collection name after FROM",
            ParserErrorCode::ExpectedIdentifier
        );
        if (!collection.has_value()) [[unlikely]] {
            return std::unexpected(collection.error());
        }

        return std::make_unique<ast::ShowVectorIndexesStatement>(
            std::string(collection->value()),
            context_.ast_location(location)
        );
    }

    [[unlikely]] return std::unexpected(context_.make_current_error(
        ParserErrorCode::ExpectedToken,
        "Expected DATABASES, COLLECTIONS, INDEXES, or VINDEXES after SHOW"
    ));
}

} // namespace litedb::core::parser

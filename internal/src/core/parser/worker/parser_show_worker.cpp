#include "core/parser/worker/parser_show_worker.hpp"

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

std::expected<std::unique_ptr<ast::StatementNode>, ParserError>
ParserShowWorker::parse_show_statement()
{
    const TokenLocation location = context_.current().location();
    context_.advance();

    if (context_.match(TokenType::Databases)) {
        return parse_show_databases_statement(location);
    }

    if (context_.match(TokenType::Collections)) {
        return parse_show_collections_statement(location);
    }

    if (context_.match(TokenType::Indexes)) {
        return parse_show_indexes_statement(location);
    }

    if (context_.match(TokenType::VIndexes)) {
        return parse_show_vector_indexes_statement(location);
    }

    [[unlikely]] return std::unexpected(context_.make_current_error(
        ParserErrorCode::ExpectedToken,
        "Expected DATABASES, COLLECTIONS, INDEXES, or VINDEXES after SHOW"
    ));
}

std::expected<std::unique_ptr<ast::StatementNode>, ParserError>
ParserShowWorker::parse_show_databases_statement(TokenLocation location)
{
    return std::make_unique<ast::ShowDatabasesStatement>(
        context_.ast_location(location)
    );
}

std::expected<std::unique_ptr<ast::StatementNode>, ParserError>
ParserShowWorker::parse_show_collections_statement(TokenLocation location)
{
    std::optional<std::string> database_name;
    if (context_.match(TokenType::From)) {
        auto database = context_.consume(
            TokenType::Identifier,
            "Expected database name after FROM",
            ParserErrorCode::ExpectedIdentifier
        );
        if (!database.has_value()) [[unlikely]] {
            return std::unexpected(std::move(database.error()));
        }
        database_name = std::string(database->value());
    }

    return std::make_unique<ast::ShowCollectionsStatement>(
        std::move(database_name),
        context_.ast_location(location)
    );
}

std::expected<std::unique_ptr<ast::StatementNode>, ParserError>
ParserShowWorker::parse_show_indexes_statement(TokenLocation location)
{
    auto from = context_.consume(
        TokenType::From, "Expected FROM after SHOW INDEXES"
    );
    if (!from.has_value()) [[unlikely]] {
        return std::unexpected(std::move(from.error()));
    }

    auto collection = context_.consume(
        TokenType::Identifier,
        "Expected collection name after FROM",
        ParserErrorCode::ExpectedIdentifier
    );
    if (!collection.has_value()) [[unlikely]] {
        return std::unexpected(std::move(collection.error()));
    }

    return std::make_unique<ast::ShowIndexesStatement>(
        std::string(collection->value()),
        context_.ast_location(location)
    );
}

std::expected<std::unique_ptr<ast::StatementNode>, ParserError>
ParserShowWorker::parse_show_vector_indexes_statement(TokenLocation location)
{
    auto from = context_.consume(
        TokenType::From, "Expected FROM after SHOW VINDEXES"
    );
    if (!from.has_value()) [[unlikely]] {
        return std::unexpected(std::move(from.error()));
    }

    auto collection = context_.consume(
        TokenType::Identifier,
        "Expected collection name after FROM",
        ParserErrorCode::ExpectedIdentifier
    );
    if (!collection.has_value()) [[unlikely]] {
        return std::unexpected(std::move(collection.error()));
    }

    return std::make_unique<ast::ShowVectorIndexesStatement>(
        std::string(collection->value()),
        context_.ast_location(location)
    );
}

} // namespace litedb::core::parser

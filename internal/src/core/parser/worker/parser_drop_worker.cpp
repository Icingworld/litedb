#include "core/parser/worker/parser_drop_worker.hpp"

#include <utility>

#include "core/parser/ast/statement/drop_collection_statement.hpp"
#include "core/parser/ast/statement/drop_database_statement.hpp"
#include "core/parser/ast/statement/drop_index_statement.hpp"
#include "core/parser/ast/statement/drop_vector_index_statement.hpp"
#include "core/parser/worker/parser_schema_helper.hpp"

namespace litedb::core::parser
{

ParserDropWorker::ParserDropWorker(ParserContext & context) noexcept
    : context_(context)
    , schema_helper_(context)
{
}

std::expected<std::unique_ptr<ast::StatementNode>, ParserError>
ParserDropWorker::parse_drop_statement()
{
    const TokenLocation location = context_.current().location();
    context_.advance();

    if (context_.match(TokenType::Database)) {
        return parse_drop_database_statement(location);
    }

    if (context_.match(TokenType::Collection)) {
        return parse_drop_collection_statement(location);
    }

    if (context_.match(TokenType::Index)) {
        return parse_drop_index_statement(location);
    }

    if (context_.match(TokenType::VIndex)) {
        return parse_drop_vector_index_statement(location);
    }

    [[unlikely]] return std::unexpected(context_.make_current_error(
        ParserErrorCode::ExpectedToken,
        "Expected DATABASE, COLLECTION, INDEX, or VINDEX after DROP"
    ));
}

std::expected<std::unique_ptr<ast::StatementNode>, ParserError>
ParserDropWorker::parse_drop_database_statement(TokenLocation location)
{
    auto if_exists = schema_helper_.parse_if_exists();
    if (!if_exists.has_value()) [[unlikely]] {
        return std::unexpected(std::move(if_exists.error()));
    }

    auto database = schema_helper_.parse_identifier_string(
        "Expected database name"
    );
    if (!database.has_value()) [[unlikely]] {
        return std::unexpected(std::move(database.error()));
    }

    return std::make_unique<ast::DropDatabaseStatement>(
        std::move(*database),
        *if_exists,
        context_.ast_location(location)
    );
}

std::expected<std::unique_ptr<ast::StatementNode>, ParserError>
ParserDropWorker::parse_drop_collection_statement(TokenLocation location)
{
    auto if_exists = schema_helper_.parse_if_exists();
    if (!if_exists.has_value()) [[unlikely]] {
        return std::unexpected(std::move(if_exists.error()));
    }

    auto collection = schema_helper_.parse_identifier_string(
        "Expected collection name"
    );
    if (!collection.has_value()) [[unlikely]] {
        return std::unexpected(std::move(collection.error()));
    }

    return std::make_unique<ast::DropCollectionStatement>(
        std::move(*collection),
        *if_exists,
        context_.ast_location(location)
    );
}

std::expected<std::unique_ptr<ast::StatementNode>, ParserError>
ParserDropWorker::parse_drop_index_statement(TokenLocation location)
{
    auto if_exists = schema_helper_.parse_if_exists();
    if (!if_exists.has_value()) [[unlikely]] {
        return std::unexpected(std::move(if_exists.error()));
    }

    auto index_name = schema_helper_.parse_identifier_string(
        "Expected index name"
    );
    if (!index_name.has_value()) [[unlikely]] {
        return std::unexpected(std::move(index_name.error()));
    }

    auto on = context_.consume(
        TokenType::On, "Expected ON after index name"
    );
    if (!on.has_value()) [[unlikely]] {
        return std::unexpected(std::move(on.error()));
    }

    auto collection_name = schema_helper_.parse_identifier_string(
        "Expected collection name"
    );
    if (!collection_name.has_value()) [[unlikely]] {
        return std::unexpected(std::move(collection_name.error()));
    }

    return std::make_unique<ast::DropIndexStatement>(
        std::move(*index_name),
        std::move(*collection_name),
        *if_exists,
        context_.ast_location(location)
    );
}

std::expected<std::unique_ptr<ast::StatementNode>, ParserError>
ParserDropWorker::parse_drop_vector_index_statement(TokenLocation location)
{
    auto if_exists = schema_helper_.parse_if_exists();
    if (!if_exists.has_value()) [[unlikely]] {
        return std::unexpected(std::move(if_exists.error()));
    }

    auto index_name = schema_helper_.parse_identifier_string(
        "Expected vector index name"
    );
    if (!index_name.has_value()) [[unlikely]] {
        return std::unexpected(std::move(index_name.error()));
    }

    auto on = context_.consume(
        TokenType::On, "Expected ON after vector index name"
    );
    if (!on.has_value()) [[unlikely]] {
        return std::unexpected(std::move(on.error()));
    }

    auto collection_name = schema_helper_.parse_identifier_string(
        "Expected collection name"
    );
    if (!collection_name.has_value()) [[unlikely]] {
        return std::unexpected(std::move(collection_name.error()));
    }

    return std::make_unique<ast::DropVectorIndexStatement>(
        std::move(*index_name),
        std::move(*collection_name),
        *if_exists,
        context_.ast_location(location)
    );
}

} // namespace litedb::core::parser

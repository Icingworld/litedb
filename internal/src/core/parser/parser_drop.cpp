#include "core/parser/parser_worker.hpp"

#include <expected>
#include <memory>
#include <utility>

#include "core/parser/ast/statement/drop_collection_statement.hpp"
#include "core/parser/ast/statement/drop_database_statement.hpp"
#include "core/parser/ast/statement/drop_index_statement.hpp"
#include "core/parser/ast/statement/drop_vector_index_statement.hpp"

namespace litedb::core::parser
{

std::expected<std::unique_ptr<ast::StatementNode>, ParserError> ParserWorker::parse_drop_statement()
{
    // 保存并消耗 DROP 关键字
    const TokenLocation location = current_token_.location();
    advance();

    if (match(TokenType::Database)) {
        // 解析 DROP DATABASE 语句

        // 判断是否存在 IF EXISTS 关键字
        auto if_exists = parse_if_exists();
        if (!if_exists.has_value()) [[unlikely]] {
            return std::unexpected(if_exists.error());
        }

        // 解析数据库名称
        auto database = parse_identifier_string("Expected database name");
        if (!database.has_value()) [[unlikely]] {
            return std::unexpected(database.error());
        }

        return std::make_unique<ast::DropDatabaseStatement>(
            std::move(database.value()),
            if_exists.value(),
            ast_location(location)
        );
    }

    if (match(TokenType::Collection)) {
        // 解析 DROP COLLECTION 语句

        // 判断是否存在 IF EXISTS 关键字
        auto if_exists = parse_if_exists();
        if (!if_exists.has_value()) [[unlikely]] {
            return std::unexpected(if_exists.error());
        }

        // 解析集合名称
        auto collection = parse_identifier_string("Expected collection name");
        if (!collection.has_value()) [[unlikely]] {
            return std::unexpected(collection.error());
        }

        return std::make_unique<ast::DropCollectionStatement>(
            std::move(collection.value()),
            if_exists.value(),
            ast_location(location)
        );
    }

    if (match(TokenType::Index)) {
        // 解析 DROP INDEX 语句

        // 判断是否存在 IF EXISTS 关键字
        auto if_exists = parse_if_exists();
        if (!if_exists.has_value()) [[unlikely]] {
            return std::unexpected(if_exists.error());
        }

        // 解析索引名称
        auto index_name = parse_identifier_string("Expected index name");
        if (!index_name.has_value()) [[unlikely]] {
            return std::unexpected(index_name.error());
        }

        // 期望 ON 关键字
        auto on = consume(TokenType::On, "Expected ON after index name");
        if (!on.has_value()) [[unlikely]] {
            return std::unexpected(on.error());
        }

        // 解析集合名称
        auto collection_name = parse_identifier_string("Expected collection name");
        if (!collection_name.has_value()) [[unlikely]] {
            return std::unexpected(collection_name.error());
        }

        return std::make_unique<ast::DropIndexStatement>(
            std::move(index_name.value()),
            std::move(collection_name.value()),
            if_exists.value(),
            ast_location(location)
        );
    }

    if (match(TokenType::VIndex)) {
        return parse_drop_vector_index_statement(location);
    }

    return std::unexpected(make_current_error(
        ParserErrorCode::ExpectedToken,
        "Expected DATABASE, COLLECTION, INDEX, or VINDEX after DROP"
    ));
}

std::expected<std::unique_ptr<ast::StatementNode>, ParserError> ParserWorker::parse_drop_vector_index_statement(
    TokenLocation location
)
{
    auto if_exists = parse_if_exists();
    if (!if_exists.has_value()) [[unlikely]] {
        return std::unexpected(if_exists.error());
    }

    auto index_name = parse_identifier_string("Expected vector index name");
    if (!index_name.has_value()) [[unlikely]] {
        return std::unexpected(index_name.error());
    }

    auto on = consume(TokenType::On, "Expected ON after vector index name");
    if (!on.has_value()) [[unlikely]] {
        return std::unexpected(on.error());
    }

    auto collection_name = parse_identifier_string("Expected collection name");
    if (!collection_name.has_value()) [[unlikely]] {
        return std::unexpected(collection_name.error());
    }

    return std::make_unique<ast::DropVectorIndexStatement>(
        std::move(index_name.value()),
        std::move(collection_name.value()),
        if_exists.value(),
        ast_location(location)
    );
}

} // namespace litedb::core::parser

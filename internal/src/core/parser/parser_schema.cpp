#include "core/parser/parser_worker.hpp"

#include <charconv>
#include <expected>
#include <memory>
#include <optional>
#include <string>
#include <string_view>
#include <system_error>
#include <utility>

#include "core/parser/ast/statement/describe_statement.hpp"
#include "core/parser/ast/statement/show_statement.hpp"
#include "core/parser/ast/statement/use_statement.hpp"
#include "core/parser/parser_helper.hpp"

namespace litedb::core::parser
{

std::expected<std::unique_ptr<ast::StatementNode>, ParserError> ParserWorker::parse_use_statement()
{
    // 保存并消耗 USE 关键字
    const TokenLocation location = current_token_.location();
    advance();

    // 解析数据库名称
    auto database = parse_identifier_string("Expected database name");
    if (!database.has_value()) [[unlikely]] {
        return std::unexpected(database.error());
    }

    return std::make_unique<ast::UseStatement>(
        std::move(database.value()),
        ast_location(location)
    );
}

std::expected<std::unique_ptr<ast::StatementNode>, ParserError> ParserWorker::parse_show_statement()
{
    // 保存并消耗 SHOW 关键字
    const TokenLocation location = current_token_.location();
    advance();

    // 解析对象类型
    auto object_type = parse_schema_object_type(true);
    if (!object_type.has_value()) [[unlikely]] {
        return std::unexpected(object_type.error());
    }

    return std::make_unique<ast::ShowStatement>(object_type.value(), ast_location(location));
}

std::expected<std::unique_ptr<ast::StatementNode>, ParserError> ParserWorker::parse_describe_statement()
{
    // 保存并消耗 DESCRIBE 关键字
    const TokenLocation location = current_token_.location();
    advance();

    // 判断是否为 COLLECTION 关键字
    if (check(TokenType::Collection)) {
        advance();
    }

    // 解析集合名称
    auto collection = parse_identifier_string("Expected collection name");
    if (!collection.has_value()) [[unlikely]] {
        return std::unexpected(collection.error());
    }

    return std::make_unique<ast::DescribeStatement>(
        ast::SchemaObjectType::Collection,
        std::move(collection.value()),
        ast_location(location)
    );
}

std::expected<std::string, ParserError> ParserWorker::parse_identifier_string(std::string_view message)
{
    // 期望标识符
    auto token = consume(TokenType::Identifier, message, ParserErrorCode::ExpectedIdentifier);
    if (!token.has_value()) [[unlikely]] {
        return std::unexpected(token.error());
    }

    return std::string(token->value());
}

std::expected<std::size_t, ParserError> ParserWorker::parse_integer_value(std::string_view message)
{
    // 期望整数字面量
    auto token = consume(TokenType::IntegerLiteral, message);
    if (!token.has_value()) [[unlikely]] {
        return std::unexpected(token.error());
    }

    std::size_t value = 0;
    const std::string_view text = token->value();
    const auto * begin = text.data();
    const auto * end = text.data() + text.size();
    const auto result = std::from_chars(begin, end, value);
    if (result.ec != std::errc {} || result.ptr != end) [[unlikely]] {
        return std::unexpected(make_parser_error(ParserErrorCode::InvalidInteger, token->location(), "Invalid integer value"));
    }

    return value;
}

std::expected<ast::DataType, ParserError> ParserWorker::parse_data_type()
{
    if (match(TokenType::Integer)) {
        return ast::DataType {ast::DataTypeKind::Integer, std::nullopt};
    }
    if (match(TokenType::BigInt)) {
        return ast::DataType {ast::DataTypeKind::BigInt, std::nullopt};
    }
    if (match(TokenType::Float)) {
        return ast::DataType {ast::DataTypeKind::Float, std::nullopt};
    }
    if (match(TokenType::Double)) {
        return ast::DataType {ast::DataTypeKind::Double, std::nullopt};
    }
    if (match(TokenType::Boolean)) {
        return ast::DataType {ast::DataTypeKind::Boolean, std::nullopt};
    }
    if (match(TokenType::Varchar)) {
        // 期望 (
        auto left_paren = consume(TokenType::LeftParen, "Expected '(' after VARCHAR");
        if (!left_paren.has_value()) [[unlikely]] {
            return std::unexpected(left_paren.error());
        }
        // 解析长度
        auto parameter = parse_integer_value("Expected VARCHAR length");
        if (!parameter.has_value()) [[unlikely]] {
            return std::unexpected(parameter.error());
        }
        // 期望 )
        auto right_paren = consume(TokenType::RightParen, "Expected ')' after VARCHAR length");
        if (!right_paren.has_value()) [[unlikely]] {
            return std::unexpected(right_paren.error());
        }
        return ast::DataType {ast::DataTypeKind::Varchar, parameter.value()};
    }
    if (match(TokenType::Vector)) {
        // 期望 (
        auto left_paren = consume(TokenType::LeftParen, "Expected '(' after VECTOR");
        if (!left_paren.has_value()) [[unlikely]] {
            return std::unexpected(left_paren.error());
        }
        // 解析维度
        auto parameter = parse_integer_value("Expected VECTOR dimension");
        if (!parameter.has_value()) [[unlikely]] {
            return std::unexpected(parameter.error());
        }
        // 期望 )
        auto right_paren = consume(TokenType::RightParen, "Expected ')' after VECTOR dimension");
        if (!right_paren.has_value()) [[unlikely]] {
            return std::unexpected(right_paren.error());
        }
        return ast::DataType {ast::DataTypeKind::Vector, parameter.value()};
    }

    return std::unexpected(make_current_error(ParserErrorCode::ExpectedDataType, "Expected data type"));
}

std::expected<ast::ColumnDefinition, ParserError> ParserWorker::parse_column_definition()
{
    // 解析列名称
    auto name = parse_identifier_string("Expected column name");
    if (!name.has_value()) [[unlikely]] {
        return std::unexpected(name.error());
    }

    // 解析数据类型
    auto type = parse_data_type();
    if (!type.has_value()) [[unlikely]] {
        return std::unexpected(type.error());
    }

    ast::ColumnDefinition column;
    column.name = std::move(name.value());
    column.type = type.value();

    // 循环解析列约束
    while (!check(TokenType::Comma) && !check(TokenType::RightParen) && !check(TokenType::EoF)) {
        // 尝试匹配 PRIMARY 关键字
        if (match(TokenType::Primary)) {
            // 期望 KEY 关键字
            auto key = consume(TokenType::Key, "Expected KEY after PRIMARY");
            if (!key.has_value()) [[unlikely]] {
                return std::unexpected(key.error());
            }
            column.primary_key = true;
        } else if (match(TokenType::Unique)) {
            column.unique = true;
        } else if (match(TokenType::Default)) {
            auto default_value = parse_literal_expression();
            if (!default_value.has_value()) [[unlikely]] {
                return std::unexpected(make_current_error(ParserErrorCode::ExpectedLiteral, "Expected literal after DEFAULT"));
            }
            column.default_value = std::move(default_value.value());
        } else if (match(TokenType::Comment)) {
            auto comment = consume(TokenType::StringLiteral, "Expected string literal after COMMENT");
            if (!comment.has_value()) [[unlikely]] {
                return std::unexpected(comment.error());
            }
            column.comment = std::string(comment->value());
        } else {
            return std::unexpected(make_current_error(ParserErrorCode::UnexpectedToken, "Unexpected column constraint"));
        }
    }

    return column;
}

std::expected<ast::SchemaObjectType, ParserError> ParserWorker::parse_schema_object_type(bool plural)
{
    // 尝试匹配 DATABASE 或 DATABASES 关键字
    if (!plural && match(TokenType::Database)) {
        return ast::SchemaObjectType::Database;
    }
    if (!plural && match(TokenType::Collection)) {
        return ast::SchemaObjectType::Collection;
    }
    if (plural && match(TokenType::Databases)) {
        return ast::SchemaObjectType::Database;
    }
    if (plural && match(TokenType::Collections)) {
        return ast::SchemaObjectType::Collection;
    }

    return std::unexpected(make_current_error(
        ParserErrorCode::ExpectedToken,
        plural ? "Expected DATABASES or COLLECTIONS" : "Expected DATABASE or COLLECTION"
    ));
}

std::expected<bool, ParserError> ParserWorker::parse_if_not_exists()
{
    if (!match(TokenType::If)) {
        return false;
    }

    auto not_token = consume(TokenType::Not, "Expected NOT after IF");
    if (!not_token.has_value()) [[unlikely]] {
        return std::unexpected(not_token.error());
    }
    auto exists_token = consume(TokenType::Exists, "Expected EXISTS after IF NOT");
    if (!exists_token.has_value()) [[unlikely]] {
        return std::unexpected(exists_token.error());
    }

    return true;
}

std::expected<bool, ParserError> ParserWorker::parse_if_exists()
{
    if (!match(TokenType::If)) {
        return false;
    }

    auto exists_token = consume(TokenType::Exists, "Expected EXISTS after IF");
    if (!exists_token.has_value()) [[unlikely]] {
        return std::unexpected(exists_token.error());
    }

    return true;
}

} // namespace litedb::core::parser

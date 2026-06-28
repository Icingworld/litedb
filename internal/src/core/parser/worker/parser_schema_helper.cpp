#include "core/parser/worker/parser_schema_helper.hpp"

#include <charconv>
#include <expected>
#include <optional>
#include <string>
#include <string_view>
#include <system_error>
#include <utility>

#include "core/parser/parser_helper.hpp"
#include "core/parser/worker/parser_expression_worker.hpp"

namespace litedb::core::parser
{

ParserSchemaHelper::ParserSchemaHelper(ParserContext & context)
    : context_(context)
    , expression_worker_(context)
{
}

std::expected<std::string, ParserError> ParserSchemaHelper::parse_identifier_string(std::string_view message)
{
    auto token = context_.consume(TokenType::Identifier, message, ParserErrorCode::ExpectedIdentifier);
    if (!token.has_value()) [[unlikely]] {
        return std::unexpected(token.error());
    }

    return std::string(token->value());
}

std::expected<std::size_t, ParserError> ParserSchemaHelper::parse_integer_value(std::string_view message)
{
    auto token = context_.consume(TokenType::IntegerLiteral, message);
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

std::expected<ast::DataType, ParserError> ParserSchemaHelper::parse_data_type()
{
    if (context_.match(TokenType::Integer)) {
        return ast::DataType {ast::DataTypeKind::Integer, std::nullopt};
    }
    if (context_.match(TokenType::BigInt)) {
        return ast::DataType {ast::DataTypeKind::BigInt, std::nullopt};
    }
    if (context_.match(TokenType::Float)) {
        return ast::DataType {ast::DataTypeKind::Float, std::nullopt};
    }
    if (context_.match(TokenType::Double)) {
        return ast::DataType {ast::DataTypeKind::Double, std::nullopt};
    }
    if (context_.match(TokenType::Boolean)) {
        return ast::DataType {ast::DataTypeKind::Boolean, std::nullopt};
    }
    if (context_.match(TokenType::Varchar)) {
        auto left_paren = context_.consume(TokenType::LeftParen, "Expected '(' after VARCHAR");
        if (!left_paren.has_value()) [[unlikely]] {
            return std::unexpected(left_paren.error());
        }
        auto parameter = parse_integer_value("Expected VARCHAR length");
        if (!parameter.has_value()) [[unlikely]] {
            return std::unexpected(parameter.error());
        }
        auto right_paren = context_.consume(TokenType::RightParen, "Expected ')' after VARCHAR length");
        if (!right_paren.has_value()) [[unlikely]] {
            return std::unexpected(right_paren.error());
        }
        return ast::DataType {ast::DataTypeKind::Varchar, parameter.value()};
    }
    if (context_.match(TokenType::Vector)) {
        auto left_paren = context_.consume(TokenType::LeftParen, "Expected '(' after VECTOR");
        if (!left_paren.has_value()) [[unlikely]] {
            return std::unexpected(left_paren.error());
        }
        auto parameter = parse_integer_value("Expected VECTOR dimension");
        if (!parameter.has_value()) [[unlikely]] {
            return std::unexpected(parameter.error());
        }
        auto right_paren = context_.consume(TokenType::RightParen, "Expected ')' after VECTOR dimension");
        if (!right_paren.has_value()) [[unlikely]] {
            return std::unexpected(right_paren.error());
        }
        return ast::DataType {ast::DataTypeKind::Vector, parameter.value()};
    }

    return std::unexpected(context_.make_current_error(ParserErrorCode::ExpectedDataType, "Expected data type"));
}

std::expected<ast::ColumnDefinition, ParserError> ParserSchemaHelper::parse_column_definition()
{
    auto name = parse_identifier_string("Expected column name");
    if (!name.has_value()) [[unlikely]] {
        return std::unexpected(name.error());
    }

    auto type = parse_data_type();
    if (!type.has_value()) [[unlikely]] {
        return std::unexpected(type.error());
    }

    ast::ColumnDefinition column;
    column.name = std::move(name.value());
    column.type = type.value();

    while (!context_.check(TokenType::Comma) && !context_.check(TokenType::RightParen) && !context_.check(TokenType::EoF)) {
        if (context_.match(TokenType::Primary)) {
            auto key = context_.consume(TokenType::Key, "Expected KEY after PRIMARY");
            if (!key.has_value()) [[unlikely]] {
                return std::unexpected(key.error());
            }
            column.primary_key = true;
        } else if (context_.match(TokenType::Unique)) {
            column.unique = true;
        } else if (context_.match(TokenType::Default)) {
            auto default_value = expression_worker_.parse_literal_expression();
            if (!default_value.has_value()) [[unlikely]] {
                return std::unexpected(context_.make_current_error(ParserErrorCode::ExpectedLiteral, "Expected literal after DEFAULT"));
            }
            column.default_value = std::move(default_value.value());
        } else if (context_.match(TokenType::Comment)) {
            auto comment = context_.consume(TokenType::StringLiteral, "Expected string literal after COMMENT");
            if (!comment.has_value()) [[unlikely]] {
                return std::unexpected(comment.error());
            }
            column.comment = std::string(comment->value());
        } else {
            return std::unexpected(context_.make_current_error(ParserErrorCode::UnexpectedToken, "Unexpected column constraint"));
        }
    }

    return column;
}

std::expected<bool, ParserError> ParserSchemaHelper::parse_if_not_exists()
{
    if (!context_.match(TokenType::If)) {
        return false;
    }

    auto not_token = context_.consume(TokenType::Not, "Expected NOT after IF");
    if (!not_token.has_value()) [[unlikely]] {
        return std::unexpected(not_token.error());
    }
    auto exists_token = context_.consume(TokenType::Exists, "Expected EXISTS after IF NOT");
    if (!exists_token.has_value()) [[unlikely]] {
        return std::unexpected(exists_token.error());
    }

    return true;
}

std::expected<bool, ParserError> ParserSchemaHelper::parse_if_exists()
{
    if (!context_.match(TokenType::If)) {
        return false;
    }

    auto exists_token = context_.consume(TokenType::Exists, "Expected EXISTS after IF");
    if (!exists_token.has_value()) [[unlikely]] {
        return std::unexpected(exists_token.error());
    }

    return true;
}

} // namespace litedb::core::parser

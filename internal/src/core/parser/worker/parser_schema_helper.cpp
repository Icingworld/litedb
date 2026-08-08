#include "core/parser/worker/parser_schema_helper.hpp"

#include <charconv>
#include <optional>
#include <string>
#include <string_view>
#include <system_error>
#include <utility>

#include "core/parser/parser_helper.hpp"
#include "core/parser/worker/parser_expression_worker.hpp"

namespace litedb::core::parser
{

ParserSchemaHelper::ParserSchemaHelper(ParserContext & context) noexcept
    : context_(context)
    , expression_worker_(context)
{
}

std::expected<std::string, ParserError> ParserSchemaHelper::parse_identifier_string(
    std::string_view message
)
{
    auto token = context_.consume(
        TokenType::Identifier,
        message,
        ParserErrorCode::ExpectedIdentifier
    );
    if (!token.has_value()) [[unlikely]] {
        return std::unexpected(std::move(token.error()));
    }

    return std::string(token->value());
}

std::expected<std::size_t, ParserError> ParserSchemaHelper::parse_integer_value(
    std::string_view message
)
{
    auto token = context_.consume(TokenType::IntegerLiteral, message);
    if (!token.has_value()) [[unlikely]] {
        return std::unexpected(std::move(token.error()));
    }

    std::size_t value = 0;
    const std::string_view text = token->value();
    const auto * begin = text.data();
    const auto * end = text.data() + text.size();
    const auto result = std::from_chars(begin, end, value);
    if (result.ec != std::errc {} || result.ptr != end) [[unlikely]] {
        return std::unexpected(make_parser_error(
            ParserErrorCode::InvalidInteger,
            token->location(),
            "Invalid integer value"
        ));
    }

    return value;
}

std::expected<common::LogicalType, ParserError> ParserSchemaHelper::parse_data_type()
{
    if (context_.match(TokenType::Integer)) {
        return common::LogicalType {
            common::LogicalTypeId::Integer,
            std::nullopt
        };
    }
    if (context_.match(TokenType::BigInt)) {
        return common::LogicalType {
            common::LogicalTypeId::BigInt,
            std::nullopt
        };
    }
    if (context_.match(TokenType::Float)) {
        return common::LogicalType {
            common::LogicalTypeId::Float,
            std::nullopt
        };
    }
    if (context_.match(TokenType::Double)) {
        return common::LogicalType {
            common::LogicalTypeId::Double,
            std::nullopt
        };
    }
    if (context_.match(TokenType::Boolean)) {
        return common::LogicalType {
            common::LogicalTypeId::Boolean,
            std::nullopt
        };
    }
    if (context_.match(TokenType::Varchar)) {
        auto left_paren = context_.consume(
            TokenType::LeftParen, "Expected '(' after VARCHAR"
        );
        if (!left_paren.has_value()) [[unlikely]] {
            return std::unexpected(std::move(left_paren.error()));
        }
        auto parameter = parse_integer_value("Expected VARCHAR length");
        if (!parameter.has_value()) [[unlikely]] {
            return std::unexpected(std::move(parameter.error()));
        }
        auto right_paren = context_.consume(
TokenType::RightParen, "Expected ')' after VARCHAR length"
        );
        if (!right_paren.has_value()) [[unlikely]] {
            return std::unexpected(std::move(right_paren.error()));
        }
        return common::LogicalType {
            common::LogicalTypeId::Varchar,
            std::make_optional(*parameter)
        };
    }
    if (context_.match(TokenType::Vector)) {
        auto left_paren = context_.consume(TokenType::LeftParen, "Expected '(' after VECTOR");
        if (!left_paren.has_value()) [[unlikely]] {
            return std::unexpected(std::move(left_paren.error()));
        }
        auto parameter = parse_integer_value("Expected VECTOR dimension");
        if (!parameter.has_value()) [[unlikely]] {
            return std::unexpected(std::move(parameter.error()));
        }
        auto right_paren = context_.consume(
            TokenType::RightParen, "Expected ')' after VECTOR dimension"
        );
        if (!right_paren.has_value()) [[unlikely]] {
            return std::unexpected(std::move(right_paren.error()));
        }
        return common::LogicalType {
            common::LogicalTypeId::Vector,
            std::make_optional(*parameter)
        };
    }

    return std::unexpected(context_.make_current_error(
        ParserErrorCode::ExpectedDataType, "Expected data type"
    ));
}

std::expected<ast::ColumnDefinitionSyntax, ParserError> ParserSchemaHelper::parse_column_definition()
{
    const auto location = context_.ast_location(context_.current().location());
    auto name = parse_identifier_string("Expected column name");
    if (!name.has_value()) [[unlikely]] {
        return std::unexpected(std::move(name.error()));
    }

    auto type = parse_data_type();
    if (!type.has_value()) [[unlikely]] {
        return std::unexpected(std::move(type.error()));
    }

    ast::ColumnDefinitionSyntax column;
    column.name = std::move(*name);
    column.type = *type;
    column.location = location;

    while (!context_.check(TokenType::Comma) && !context_.check(TokenType::RightParen) && !context_.check(TokenType::EoF)) {
        if (context_.match(TokenType::Unique)) {
            column.unique = true;
        } else if (context_.match(TokenType::Default)) {
            auto default_value = expression_worker_.parse_literal_expression();
            if (!default_value.has_value()) [[unlikely]] {
                return std::unexpected(context_.make_current_error(
                    ParserErrorCode::ExpectedLiteral, "Expected literal after DEFAULT"
                ));
            }
            column.default_value = std::move(default_value->expression);
        } else if (context_.match(TokenType::Not)) {
            auto null_token = context_.consume(TokenType::Null, "Expected NULL after NOT");
            if (!null_token.has_value()) [[unlikely]] {
                return std::unexpected(std::move(null_token.error()));
            }
            column.nullable = false;
        } else if (context_.match(TokenType::Null)) {
            column.nullable = true;
            // NULL 是默认行为，不需要额外处理
        } else if (context_.match(TokenType::Comment)) {
            auto comment = context_.consume(
                TokenType::StringLiteral, "Expected string literal after COMMENT"
            );
            if (!comment.has_value()) [[unlikely]] {
                return std::unexpected(std::move(comment.error()));
            }
            column.comment = std::string(comment->value());
        } else {
            return std::unexpected(context_.make_current_error(
                ParserErrorCode::UnexpectedToken, "Unexpected column constraint"
            ));
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
        return std::unexpected(std::move(not_token.error()));
    }
    auto exists_token = context_.consume(TokenType::Exists, "Expected EXISTS after IF NOT");
    if (!exists_token.has_value()) [[unlikely]] {
        return std::unexpected(std::move(exists_token.error()));
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
        return std::unexpected(std::move(exists_token.error()));
    }

    return true;
}

} // namespace litedb::core::parser

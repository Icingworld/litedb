#include "core/parser/token.hpp"

namespace litedb::core::parser
{

Token::Token(
    TokenType type,
    std::string_view value,
    std::size_t line,
    std::size_t column
)
    : type_(type)
    , value_(value)
    , location_({line, column})
{
}

Token::Token(TokenType type, std::string_view value, TokenLocation location)
    : type_(type)
    , value_(value)
    , location_(location)
{
}

TokenType Token::type() const noexcept
{
    return type_;
}

std::string_view Token::value() const noexcept
{
    return value_;
}

TokenLocation Token::location() const noexcept
{
    return location_;
}

bool is_comparison_operator(TokenType type) noexcept
{
    return type == TokenType::Equal
        || type == TokenType::NotEqual
        || type == TokenType::LessThan
        || type == TokenType::LessEqual
        || type == TokenType::GreaterThan
        || type == TokenType::GreaterEqual;
}

bool is_literal_token(TokenType type) noexcept
{
    return type == TokenType::IntegerLiteral
        || type == TokenType::FloatLiteral
        || type == TokenType::StringLiteral
        || type == TokenType::True
        || type == TokenType::False
        || type == TokenType::Null;
}

} // namespace litedb::core::parser

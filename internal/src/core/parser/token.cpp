#include "core/parser/token.hpp"

namespace litedb::core::parser
{

Token::Token(TokenType type, std::string_view value, std::size_t line, std::size_t column)
    : type_(type)
    , value_(value)
    , location_{line, column}
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

} // namespace litedb::core::parser

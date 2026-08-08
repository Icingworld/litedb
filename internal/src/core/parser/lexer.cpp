#include "core/parser/lexer.hpp"

#include <cctype>
#include <string_view>
#include <utility>

namespace litedb::core::parser
{

Lexer::Lexer(std::string input)
    : input_(std::move(input))
    , position_(0)
    , location_({1, 1})
    , peeked_token_(std::nullopt)
{}

Token Lexer::next()
{
    // 如果已经预读，直接返回预读的 Token
    if (peeked_token_.has_value()) {
        const Token token = *peeked_token_;
        peeked_token_ = std::nullopt;
        return token;
    }

    // 没有预读，尝试获取下一个 Token
    return next_internal();
}

const Token & Lexer::peek()
{
    if (!peeked_token_.has_value()) {
        peeked_token_ = next_internal();
    }

    return *peeked_token_;
}

bool Lexer::has_more() const noexcept
{
    if (peeked_token_.has_value()) {
        return peeked_token_->type() != TokenType::EoF;
    }

    std::size_t position = position_;
    while (position < input_.length()) {
        if (!std::isspace(static_cast<unsigned char>(input_[position]))) {
            return true;
        }
        ++position;
    }

    return false;
}

TokenLocation Lexer::location() const noexcept
{
    return location_;
}

Token Lexer::next_internal()
{
    // 跳过空白字符
    skip_whitespace();

    // 如果已经到达输入末尾，返回结束标记
    if (position_ >= input_.length()) {
        return Token(TokenType::EoF, "", location_);
    }

    const char c = current_char();

    if (is_alpha(c) || c == '_') {
        return read_identifier_or_keyword();
    }

    if (is_digit(c)) {
        return read_number();
    }

    if (c == '\'' || c == '"') {
        return read_string();
    }

    const TokenLocation start = location_;
    const std::size_t start_position = position_;
    advance();

    switch (c) {
    case '=':
        return Token(TokenType::Equal, std::string_view(input_).substr(start_position, 1), start);
    case '!':
        if (match('=')) {
            return Token(
                TokenType::NotEqual,
                std::string_view(input_).substr(start_position, 2),
                start
            );
        }
        return Token(TokenType::Error, std::string_view(input_).substr(start_position, 1), start);
    case '<':
        if (match('=')) {
            return Token(
                TokenType::LessEqual,
                std::string_view(input_).substr(start_position, 2),
                start
            );
        }
        if (match('>')) {
            return Token(
                TokenType::NotEqual,
                std::string_view(input_).substr(start_position, 2),
                start
            );
        }
        return Token(
            TokenType::LessThan,
            std::string_view(input_).substr(start_position, 1),
            start
        );
    case '>':
        if (match('=')) {
            return Token(
                TokenType::GreaterEqual,
                std::string_view(input_).substr(start_position, 2),
                start
            );
        }
        return Token(
            TokenType::GreaterThan,
            std::string_view(input_).substr(start_position, 1),
            start
        );
    case '+':
        return Token(TokenType::Plus, std::string_view(input_).substr(start_position, 1), start);
    case '-':
        return Token(TokenType::Minus, std::string_view(input_).substr(start_position, 1), start);
    case '*':
        return Token(TokenType::Star, std::string_view(input_).substr(start_position, 1), start);
    case '/':
        return Token(TokenType::Slash, std::string_view(input_).substr(start_position, 1), start);
    case '%':
        return Token(TokenType::Modulo, std::string_view(input_).substr(start_position, 1), start);
    case ',':
        return Token(TokenType::Comma, std::string_view(input_).substr(start_position, 1), start);
    case ';':
        return Token(
            TokenType::Semicolon,
            std::string_view(input_).substr(start_position, 1),
            start
        );
    case '.':
        return Token(TokenType::Dot, std::string_view(input_).substr(start_position, 1), start);
    case '(':
        return Token(
            TokenType::LeftParen,
            std::string_view(input_).substr(start_position, 1),
            start
        );
    case ')':
        return Token(
            TokenType::RightParen,
            std::string_view(input_).substr(start_position, 1),
            start
        );
    case '[':
        return Token(
            TokenType::LeftBracket,
            std::string_view(input_).substr(start_position, 1),
            start
        );
    case ']':
        return Token(
            TokenType::RightBracket,
            std::string_view(input_).substr(start_position, 1),
            start
        );
    default:
        return Token(TokenType::Error, std::string_view(input_).substr(start_position, 1), start);
    }
}

void Lexer::skip_whitespace()
{
    while (position_ < input_.length()) {
        if (!std::isspace(static_cast<unsigned char>(current_char()))) {
            break;
        }
        advance();
    }
}

Token Lexer::read_identifier_or_keyword()
{
    const TokenLocation start = location_;
    const std::size_t start_position = position_;

    while (is_alnum(current_char()) || current_char() == '_') {
        advance();
    }

    const std::string_view value =
        std::string_view(input_).substr(start_position, position_ - start_position);
    std::string upper_value;
    upper_value.reserve(value.length());
    for (const char c : value) {
        upper_value.push_back(static_cast<char>(std::toupper(static_cast<unsigned char>(c))));
    }

    if (const auto type = keyword_type(upper_value); type.has_value()) {
        return Token(*type, value, start);
    }

    return Token(TokenType::Identifier, value, start);
}

Token Lexer::read_number()
{
    const TokenLocation start = location_;
    const std::size_t start_position = position_;

    while (is_digit(current_char())) {
        advance();
    }

    TokenType type = TokenType::IntegerLiteral;
    if (current_char() == '.' && position_ + 1 < input_.length() &&
        is_digit(input_[position_ + 1])) {
        type = TokenType::FloatLiteral;
        advance();
        while (is_digit(current_char())) {
            advance();
        }
    }

    return Token(
        type,
        std::string_view(input_).substr(start_position, position_ - start_position),
        start
    );
}

Token Lexer::read_string()
{
    const TokenLocation start = location_;
    const std::size_t quote_position = position_;
    const char quote = advance();
    const std::size_t value_position = position_;

    while (position_ < input_.length()) {
        const char c = current_char();
        if (c == quote) {
            const std::size_t value_length = position_ - value_position;
            advance();
            return Token(
                TokenType::StringLiteral,
                std::string_view(input_).substr(value_position, value_length),
                start
            );
        }
        if (c == '\\' && position_ + 1 < input_.length()) {
            advance();
        }
        advance();
    }

    return Token(
        TokenType::Error,
        std::string_view(input_).substr(quote_position, position_ - quote_position),
        start
    );
}

bool Lexer::is_alpha(char c) const noexcept
{
    return std::isalpha(static_cast<unsigned char>(c)) != 0;
}

bool Lexer::is_digit(char c) const noexcept
{
    return std::isdigit(static_cast<unsigned char>(c)) != 0;
}

bool Lexer::is_alnum(char c) const noexcept
{
    return std::isalnum(static_cast<unsigned char>(c)) != 0;
}

char Lexer::current_char() const noexcept
{
    if (position_ >= input_.length()) {
        return '\0';
    }
    return input_[position_];
}

char Lexer::advance() noexcept
{
    const char c = current_char();
    if (position_ < input_.length()) {
        ++position_;
        if (c == '\n') {
            ++location_.line;
            location_.column = 1;
        } else {
            ++location_.column;
        }
    }
    return c;
}

bool Lexer::match(char expected)
{
    if (current_char() == expected) {
        advance();
        return true;
    }
    return false;
}

} // namespace litedb::core::parser

#include "core/parser/parser_context.hpp"

#include <expected>
#include <string_view>

#include "core/parser/lexer.hpp"
#include "core/parser/parser_helper.hpp"

namespace litedb::core::parser
{

ParserContext::ParserContext(Lexer & lexer)
    : lexer_(lexer)
    , current_token_(TokenType::EoF, "", TokenLocation {1, 1})
    , next_token_(TokenType::EoF, "", TokenLocation {1, 1})
    , next_after_next_token_(TokenType::EoF, "", TokenLocation {1, 1})
{
}

void ParserContext::initialize()
{
    current_token_ = lexer_.next();
    next_token_ = lexer_.next();
    next_after_next_token_ = lexer_.next();
}

const Token & ParserContext::current() const noexcept
{
    return current_token_;
}

const Token & ParserContext::peek_next() const noexcept
{
    return next_token_;
}

const Token & ParserContext::peek_after_next() const noexcept
{
    return next_after_next_token_;
}

Token ParserContext::advance()
{
    const Token previous = current_token_;
    current_token_ = next_token_;
    next_token_ = next_after_next_token_;
    next_after_next_token_ = lexer_.next();
    return previous;
}

bool ParserContext::match(TokenType type)
{
    if (!check(type)) {
        return false;
    }

    advance();
    return true;
}

bool ParserContext::check(TokenType type) const
{
    return current_token_.type() == type;
}

ParserError ParserContext::make_current_error(
    ParserErrorCode code,
    std::string_view message
) const
{
    if (current_token_.type() == TokenType::Error) {
        return make_parser_error(
            ParserErrorCode::LexicalError,
            current_token_.location(),
            "Invalid token"
        );
    }

    return make_parser_error(code, current_token_.location(), message);
}

std::expected<Token, ParserError> ParserContext::consume(
    TokenType type,
    std::string_view message,
    ParserErrorCode code
)
{
    if (!check(type)) [[unlikely]] {
        return std::unexpected(make_current_error(code, message));
    }

    return advance();
}

void ParserContext::skip_semicolon()
{
    if (check(TokenType::Semicolon)) {
        advance();
    }
}

ast::AstNodeLocation ParserContext::ast_location(
    TokenLocation location
) const noexcept
{
    return ast::AstNodeLocation {location.line, location.column};
}

} // namespace litedb::core::parser

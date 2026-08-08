#include "core/parser/parser_context.hpp"

#include <expected>
#include <string_view>

#include "core/parser/lexer.hpp"
#include "core/parser/parser_helper.hpp"
#include "core/parser/parser_limits.hpp"

namespace litedb::core::parser
{

ParserContext::ExpressionNestingGuard::ExpressionNestingGuard() noexcept
    : context_(nullptr)
{}

ParserContext::ExpressionNestingGuard::ExpressionNestingGuard(ParserContext & context) noexcept
    : context_(&context)
{}

ParserContext::ExpressionNestingGuard::ExpressionNestingGuard(
    ExpressionNestingGuard && other
) noexcept
    : context_(std::exchange(other.context_, nullptr))
{}

ParserContext::ExpressionNestingGuard & ParserContext::ExpressionNestingGuard::operator=(
    ExpressionNestingGuard && other
) noexcept
{
    if (this == &other) {
        return *this;
    }

    if (context_ != nullptr) {
        context_->leave_expression_nesting();
    }
    context_ = std::exchange(other.context_, nullptr);
    return *this;
}

ParserContext::ExpressionNestingGuard::~ExpressionNestingGuard()
{
    if (context_ != nullptr) {
        context_->leave_expression_nesting();
    }
}

ParserContext::ParserContext(Lexer & lexer) noexcept
    : lexer_(lexer)
    , current_token_(TokenType::EoF, "", TokenLocation {1, 1})
    , next_token_(TokenType::EoF, "", TokenLocation {1, 1})
    , next_after_next_token_(TokenType::EoF, "", TokenLocation {1, 1})
{}

void ParserContext::initialize()
{
    expression_nesting_depth_ = 0;
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

ParserError ParserContext::make_current_error(ParserErrorCode code, std::string_view message) const
{
    if (current_token_.type() == TokenType::Error) {
        return make_parser_error(
            ParserErrorCode::LexicalError,
            current_token_.location(),
            "Invalid token"
        );
    }

    return make_error(code, current_token_.location(), message);
}

ParserError ParserContext::make_error(
    ParserErrorCode code,
    TokenLocation location,
    std::string_view message
) const
{
    return make_parser_error(code, location, message);
}

std::expected<ParserContext::ExpressionNestingGuard, ParserError>
ParserContext::enter_expression_nesting(TokenLocation location)
{
    if (expression_nesting_limit_reached()) [[unlikely]] {
        return std::unexpected(make_expression_nesting_error(location));
    }

    ++expression_nesting_depth_;
    return ExpressionNestingGuard(*this);
}

bool ParserContext::expression_nesting_limit_reached() const noexcept
{
    return expression_nesting_depth_ >= MaxExpressionDepth;
}

ParserError ParserContext::make_expression_nesting_error(TokenLocation location) const
{
    return make_error(
        ParserErrorCode::ExpressionDepthLimitExceeded,
        location,
        "Maximum expression nesting depth exceeded (limit: 256)"
    );
}

std::expected<std::size_t, ParserError> ParserContext::make_expression_parent_depth(
    std::size_t max_child_depth,
    TokenLocation location
) const
{
    if (max_child_depth >= MaxExpressionDepth) [[unlikely]] {
        return std::unexpected(make_error(
            ParserErrorCode::ExpressionDepthLimitExceeded,
            location,
            "Maximum AST expression depth exceeded (limit: 256)"
        ));
    }

    return max_child_depth + 1;
}

void ParserContext::leave_expression_nesting() noexcept
{
    --expression_nesting_depth_;
}

std::expected<Token, ParserError>
ParserContext::consume(TokenType type, std::string_view message, ParserErrorCode code)
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

ast::AstNodeLocation ParserContext::ast_location(TokenLocation location) const noexcept
{
    return ast::AstNodeLocation {location.line, location.column};
}

} // namespace litedb::core::parser

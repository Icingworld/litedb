#include "core/parser/parser_worker.hpp"

#include <expected>
#include <memory>
#include <string_view>

#include "core/parser/ast/statement/statement_node.hpp"
#include "core/parser/lexer.hpp"
#include "core/parser/parser_helper.hpp"

namespace litedb::core::parser
{

std::expected<std::unique_ptr<ast::StatementNode>, ParserError> ParserWorker::parse()
{
    current_token_ = lexer_.next();

    // 检查是否出现空语句或错误 Token 
    if (current_token_.type() == TokenType::EoF) [[unlikely]] {
        return std::unexpected(make_current_error(ParserErrorCode::EmptyStatement, "Empty statement"));
    }
    if (current_token_.type() == TokenType::Error) [[unlikely]] {
        return std::unexpected(make_current_error(ParserErrorCode::LexicalError, "Invalid token"));
    }

    // 解析语句
    auto statement = parse_statement();
    if (!statement.has_value()) [[unlikely]] {
        return std::unexpected(statement.error());
    }

    // 跳过分号
    skip_semicolon();

    // 检查是否出现错误 Token 
    if (current_token_.type() == TokenType::Error) [[unlikely]] {
        return std::unexpected(make_current_error(ParserErrorCode::LexicalError, "Invalid token"));
    }
    if (current_token_.type() != TokenType::EoF) [[unlikely]] {
        return std::unexpected(make_current_error(ParserErrorCode::UnexpectedToken, "Unexpected token"));
    }

    return statement;
}

std::expected<std::unique_ptr<ast::StatementNode>, ParserError> ParserWorker::parse_statement()
{
    // 根据当前 Token 类型分发到不同的解析
    switch (current_token_.type()) {
    case TokenType::Use:
        return parse_use_statement();
    case TokenType::Create:
        return parse_create_statement();
    case TokenType::Drop:
        return parse_drop_statement();
    case TokenType::Show:
        return parse_show_statement();
    case TokenType::Describe:
        [[fallthrough]];
    case TokenType::Desc:
        return parse_describe_statement();
    case TokenType::Insert:
        return parse_insert_statement();
    case TokenType::Update:
        return parse_update_statement();
    case TokenType::Delete:
        return parse_delete_statement();
    case TokenType::Select:
        return parse_select_statement();
    [[unlikely]] default:
        return std::unexpected(make_current_error(ParserErrorCode::UnexpectedStatement, "Unexpected statement"));
    }
}

Token ParserWorker::advance()
{
    const Token previous = current_token_;
    current_token_ = lexer_.next();
    return previous;
}

bool ParserWorker::match(TokenType type)
{
    if (!check(type)) {
        return false;
    }

    advance();
    return true;
}

bool ParserWorker::check(TokenType type) const
{
    return current_token_.type() == type;
}

ParserError ParserWorker::make_current_error(ParserErrorCode code, std::string_view message) const
{
    if (current_token_.type() == TokenType::Error) {
        return make_parser_error(ParserErrorCode::LexicalError, current_token_.location(), "Invalid token");
    }

    return make_parser_error(code, current_token_.location(), message);
}

std::expected<Token, ParserError> ParserWorker::consume(
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

void ParserWorker::skip_semicolon()
{
    if (check(TokenType::Semicolon)) {
        advance();
    }
}

ast::AstNodeLocation ParserWorker::ast_location(TokenLocation location) const noexcept
{
    return ast::AstNodeLocation {location.line, location.column};
}

ParserWorker::ParserWorker(Lexer & lexer)
    : lexer_(lexer)
    , current_token_(TokenType::EoF, "", TokenLocation {1, 1})
{
}

} // namespace litedb::core::parser

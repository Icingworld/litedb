#include "core/parser/worker/parser_worker.hpp"

#include <expected>
#include <memory>
#include <string_view>

#include "core/parser/ast/statement/statement_node.hpp"
#include "core/parser/lexer.hpp"
#include "core/parser/worker/parser_create_worker.hpp"
#include "core/parser/worker/parser_delete_worker.hpp"
#include "core/parser/worker/parser_describe_worker.hpp"
#include "core/parser/worker/parser_drop_worker.hpp"
#include "core/parser/worker/parser_insert_worker.hpp"
#include "core/parser/worker/parser_select_worker.hpp"
#include "core/parser/worker/parser_show_worker.hpp"
#include "core/parser/worker/parser_update_worker.hpp"
#include "core/parser/worker/parser_use_worker.hpp"

namespace litedb::core::parser
{

ParserWorker::ParserWorker(Lexer & lexer)
    : context_(lexer)
{
}

std::expected<std::unique_ptr<ast::StatementNode>, ParserError> ParserWorker::parse()
{
    context_.initialize();

    // 检查是否为空语句或词法错误
    if (context_.current().type() == TokenType::EoF) [[unlikely]] {
        return std::unexpected(context_.make_current_error(ParserErrorCode::EmptyStatement, "Empty statement"));
    }
    if (context_.current().type() == TokenType::Error) [[unlikely]] {
        return std::unexpected(context_.make_current_error(ParserErrorCode::LexicalError, "Invalid token"));
    }

    // 解析语句
    auto statement = parse_statement();
    if (!statement.has_value()) [[unlikely]] {
        return std::unexpected(std::move(statement.error()));
    }

    // 跳过分号
    context_.skip_semicolon();

    // 主工作器统一处理语句后的非法尾随 token
    if (context_.current().type() == TokenType::Error) [[unlikely]] {
        return std::unexpected(context_.make_current_error(ParserErrorCode::LexicalError, "Invalid token"));
    }
    if (context_.current().type() != TokenType::EoF) [[unlikely]] {
        return std::unexpected(context_.make_current_error(ParserErrorCode::UnexpectedToken, "Unexpected token"));
    }

    return statement;
}

std::expected<std::unique_ptr<ast::StatementNode>, ParserError> ParserWorker::parse_statement()
{
    switch (context_.current().type()) {
    case TokenType::Use: {
        return ParserUseWorker(context_).parse_use_statement();
    }
    case TokenType::Create: {
        return ParserCreateWorker(context_).parse_create_statement();
    }
    case TokenType::Drop: {
        return ParserDropWorker(context_).parse_drop_statement();
    }
    case TokenType::Show: {
        return ParserShowWorker(context_).parse_show_statement();
    }
    case TokenType::Describe:
        [[fallthrough]];
    case TokenType::Desc: {
        return ParserDescribeWorker(context_).parse_describe_statement();
    }
    case TokenType::Insert: {
        return ParserInsertWorker(context_).parse_insert_statement();
    }
    case TokenType::Update: {
        return ParserUpdateWorker(context_).parse_update_statement();
    }
    case TokenType::Delete: {
        return ParserDeleteWorker(context_).parse_delete_statement();
    }
    case TokenType::Select: {
        return ParserSelectWorker(context_).parse_select_statement();
    }
    [[unlikely]] default:
        return std::unexpected(context_.make_current_error(ParserErrorCode::UnexpectedStatement, "Unexpected statement"));
    }
}

} // namespace litedb::core::parser

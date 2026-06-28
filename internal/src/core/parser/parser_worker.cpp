#include "core/parser/parser_worker.hpp"

#include <expected>
#include <memory>
#include <string_view>

#include "core/parser/ast/statement/statement_node.hpp"
#include "core/parser/lexer.hpp"
#include "core/parser/parser_create_worker.hpp"
#include "core/parser/parser_drop_worker.hpp"
#include "core/parser/parser_mutation_worker.hpp"
#include "core/parser/parser_schema_worker.hpp"
#include "core/parser/parser_select_worker.hpp"

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

    auto statement = parse_statement();
    if (!statement.has_value()) [[unlikely]] {
        return std::unexpected(statement.error());
    }

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
        ParserSchemaWorker schema_worker(context_);
        return schema_worker.parse_use_statement();
    }
    case TokenType::Create: {
        ParserCreateWorker create_worker(context_);
        return create_worker.parse_create_statement();
    }
    case TokenType::Drop: {
        ParserDropWorker drop_worker(context_);
        return drop_worker.parse_drop_statement();
    }
    case TokenType::Show: {
        ParserSchemaWorker schema_worker(context_);
        return schema_worker.parse_show_statement();
    }
    case TokenType::Describe:
        [[fallthrough]];
    case TokenType::Desc: {
        ParserSchemaWorker schema_worker(context_);
        return schema_worker.parse_describe_statement();
    }
    case TokenType::Insert: {
        ParserMutationWorker mutation_worker(context_);
        return mutation_worker.parse_insert_statement();
    }
    case TokenType::Update: {
        ParserMutationWorker mutation_worker(context_);
        return mutation_worker.parse_update_statement();
    }
    case TokenType::Delete: {
        ParserMutationWorker mutation_worker(context_);
        return mutation_worker.parse_delete_statement();
    }
    case TokenType::Select: {
        ParserSelectWorker select_worker(context_);
        return select_worker.parse_select_statement();
    }
    [[unlikely]] default:
        return std::unexpected(context_.make_current_error(ParserErrorCode::UnexpectedStatement, "Unexpected statement"));
    }
}

} // namespace litedb::core::parser

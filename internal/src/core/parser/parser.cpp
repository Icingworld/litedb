#include "core/parser/parser.hpp"

#include <expected>
#include <memory>
#include <string>
#include <utility>

#include "core/parser/ast/statement/statement_node.hpp"
#include "core/parser/lexer.hpp"
#include "core/parser/worker/parser_worker.hpp"

namespace litedb::core::parser
{

Parser::Parser(std::string input)
    : lexer_(std::make_unique<Lexer>(std::move(input)))
{}

Parser::Parser(std::unique_ptr<Lexer> lexer)
    : lexer_(std::move(lexer))
{}

Parser::~Parser() = default;

std::expected<std::unique_ptr<ast::StatementNode>, ParserError> Parser::parse()
{
    return ParserWorker(*lexer_).parse();
}

} // namespace litedb::core::parser

#pragma once

#include <expected>
#include <memory>
#include <string>

#include "core/parser/parser_error.hpp"

namespace litedb::core::parser
{

class Lexer;

namespace ast
{

class StatementNode;

} // namespace ast

// 解析器
class Parser
{
public:
    explicit Parser(std::string input);

    explicit Parser(std::unique_ptr<Lexer> lexer);

    Parser(const Parser &) = delete;

    Parser & operator=(const Parser &) = delete;

    Parser(Parser &&) noexcept = default;

    Parser & operator=(Parser &&) noexcept = default;

    ~Parser();

public:
    // 解析 SQL 语句
    [[nodiscard]]
    std::expected<std::unique_ptr<ast::StatementNode>, ParserError> parse();

private:
    std::unique_ptr<Lexer> lexer_;
};

} // namespace litedb::core::parser

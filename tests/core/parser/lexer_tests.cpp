#include "core/parser/lexer.hpp"

#include <exception>
#include <iostream>
#include <stdexcept>
#include <string_view>

namespace
{

using litedb::core::parser::Lexer;
using litedb::core::parser::Token;
using litedb::core::parser::TokenType;

void require(bool condition, const char * message)
{
    if (!condition) {
        throw std::runtime_error(message);
    }
}

void require_token(const Token & token, TokenType type, std::string_view value, std::size_t line, std::size_t column)
{
    require(token.type() == type, "unexpected token type");
    require(token.value() == value, "unexpected token value");
    require(token.location().line == line, "unexpected token line");
    require(token.location().column == column, "unexpected token column");
}

void test_keywords_identifiers_and_peek()
{
    Lexer lexer("select name FROM users");

    const Token & peeked = lexer.peek();
    require_token(peeked, TokenType::Select, "select", 1, 1);
    require_token(lexer.next(), TokenType::Select, "select", 1, 1);
    require_token(lexer.next(), TokenType::Identifier, "name", 1, 8);
    require_token(lexer.next(), TokenType::From, "FROM", 1, 13);
    require_token(lexer.next(), TokenType::Identifier, "users", 1, 18);
    require(!lexer.has_more(), "lexer should have no more non-whitespace input");
    require_token(lexer.next(), TokenType::EoF, "", 1, 23);
}

void test_literals_and_punctuation()
{
    Lexer lexer("INSERT INTO t VALUES (12, 3.5, 'hi\\'x');");

    require_token(lexer.next(), TokenType::Insert, "INSERT", 1, 1);
    require_token(lexer.next(), TokenType::Into, "INTO", 1, 8);
    require_token(lexer.next(), TokenType::Identifier, "t", 1, 13);
    require_token(lexer.next(), TokenType::Values, "VALUES", 1, 15);
    require_token(lexer.next(), TokenType::LeftParen, "(", 1, 22);
    require_token(lexer.next(), TokenType::IntegerLiteral, "12", 1, 23);
    require_token(lexer.next(), TokenType::Comma, ",", 1, 25);
    require_token(lexer.next(), TokenType::FloatLiteral, "3.5", 1, 27);
    require_token(lexer.next(), TokenType::Comma, ",", 1, 30);
    require_token(lexer.next(), TokenType::StringLiteral, "hi\\'x", 1, 32);
    require_token(lexer.next(), TokenType::RightParen, ")", 1, 39);
    require_token(lexer.next(), TokenType::Semicolon, ";", 1, 40);
}

void test_operators_and_locations()
{
    Lexer lexer("a != b\nc <= 10 <> d >= 2");

    require_token(lexer.next(), TokenType::Identifier, "a", 1, 1);
    require_token(lexer.next(), TokenType::NotEqual, "!=", 1, 3);
    require_token(lexer.next(), TokenType::Identifier, "b", 1, 6);
    require_token(lexer.next(), TokenType::Identifier, "c", 2, 1);
    require_token(lexer.next(), TokenType::LessEqual, "<=", 2, 3);
    require_token(lexer.next(), TokenType::IntegerLiteral, "10", 2, 6);
    require_token(lexer.next(), TokenType::NotEqual, "<>", 2, 9);
    require_token(lexer.next(), TokenType::Identifier, "d", 2, 12);
    require_token(lexer.next(), TokenType::GreaterEqual, ">=", 2, 14);
    require_token(lexer.next(), TokenType::IntegerLiteral, "2", 2, 17);
}

void test_errors()
{
    Lexer lexer("! 'unterminated");

    require_token(lexer.next(), TokenType::Error, "!", 1, 1);
    require_token(lexer.next(), TokenType::Error, "'unterminated", 1, 3);
}

} // namespace

int main()
{
    try {
        test_keywords_identifiers_and_peek();
        test_literals_and_punctuation();
        test_operators_and_locations();
        test_errors();
    } catch (const std::exception & exception) {
        std::cerr << exception.what() << '\n';
        return 1;
    }

    return 0;
}

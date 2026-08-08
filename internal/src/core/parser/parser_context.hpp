#pragma once

#include <expected>
#include <string_view>

#include "core/parser/ast/ast_node.hpp"
#include "core/parser/parser_error.hpp"
#include "core/parser/token.hpp"

namespace litedb::core::parser
{

class Lexer;

// Parser 共享 Token 游标和诊断上下文
class ParserContext
{
public:
    explicit ParserContext(Lexer & lexer) noexcept;

public:
    // 初始化上下文并读取第一个 Token
    void initialize();

    // 获取当前 Token
    [[nodiscard]]
    const Token & current() const noexcept;

    // 查看下一个 Token
    [[nodiscard]]
    const Token & peek_next() const noexcept;

    // 查看下下个 Token
    [[nodiscard]]
    const Token & peek_after_next() const noexcept;

    // 前进一个 Token
    Token advance();

    // 匹配并消费指定 Token 类型
    bool match(TokenType type);

    // 检查当前 Token 类型
    [[nodiscard]]
    bool check(TokenType type) const;

    // 基于当前位置创建解析错误
    [[nodiscard]]
    ParserError make_current_error(
        ParserErrorCode code,
        std::string_view message
    ) const;

    // 消费指定类型的 Token，如果匹配失败，返回指定的错误
    [[nodiscard]]
    std::expected<Token, ParserError> consume(
        TokenType type,
        std::string_view message,
        ParserErrorCode code = ParserErrorCode::ExpectedToken
    );

    // 跳过一个可选分号
    void skip_semicolon();

    // 从 Token 位置创建 AST 节点位置
    [[nodiscard]]
    ast::AstNodeLocation ast_location(TokenLocation location) const noexcept;

private:
    Lexer & lexer_;                 // 词法分析器
    Token current_token_;           // 当前 Token
    Token next_token_;              // 下一个 Token
    Token next_after_next_token_;   // 下下个 Token
};

} // namespace litedb::core::parser

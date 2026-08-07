#pragma once

#include <expected>
#include <string_view>

#include "core/parser/ast/ast_node.hpp"
#include "core/parser/parser_error.hpp"
#include "core/parser/token.hpp"

namespace litedb::core::parser
{

class Lexer;

/**
 * @brief Parser 共享 Token 游标和诊断上下文
 */
class ParserContext
{
public:
    explicit ParserContext(Lexer & lexer);

public:
    /**
     * @brief 初始化上下文并读取第一个 Token
     */
    void initialize();

    /**
     * @brief 获取当前 Token
     * @return 当前 Token
     */
    [[nodiscard]]
    const Token & current() const noexcept;

    /**
     * @brief 查看下一个 Token
     * @return 下一个 Token
     */
    [[nodiscard]]
    const Token & peek_next() const noexcept;

    /**
     * @brief 查看下下个 Token
     * @return 下下个 Token
     */
    [[nodiscard]]
    const Token & peek_after_next() const noexcept;

    /**
     * @brief 前进一个 Token
     * @return 前进前的 Token
     */
    Token advance();

    /**
     * @brief 匹配并消费指定 Token 类型
     * @param type Token 类型
     * @return 是否匹配
     */
    bool match(TokenType type);

    /**
     * @brief 检查当前 Token 类型
     * @param type Token 类型
     * @return 是否匹配
     */
    [[nodiscard]]
    bool check(TokenType type) const;

    /**
     * @brief 基于当前位置创建解析错误
     * @param code 错误码
     * @param message 错误消息
     * @return 解析错误
     */
    [[nodiscard]]
    ParserError make_current_error(
        ParserErrorCode code,
        std::string_view message
    ) const;

    /**
     * @brief 消费指定类型的 Token
     * @param type Token 类型
     * @param message 错误消息
     * @param code 错误码
     * @return 消费到的 Token
     */
    [[nodiscard]]
    std::expected<Token, ParserError> consume(
        TokenType type,
        std::string_view message,
        ParserErrorCode code = ParserErrorCode::ExpectedToken
    );

    /**
     * @brief 跳过一个可选分号
     */
    void skip_semicolon();

    /**
     * @brief 从 Token 位置创建 AST 节点位置
     * @param location Token 位置
     * @return AST 节点位置
     */
    [[nodiscard]]
    ast::AstNodeLocation ast_location(TokenLocation location) const noexcept;

private:
    Lexer & lexer_;                 // 词法分析器
    Token current_token_;           // 当前 Token
    Token next_token_;              // 下一个 Token
    Token next_after_next_token_;   // 下下个 Token
};

} // namespace litedb::core::parser

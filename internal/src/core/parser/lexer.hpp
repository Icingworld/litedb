#pragma once

#include <optional>
#include <string>

#include "core/parser/token.hpp"

namespace litedb::core::parser
{

// 词法分析器
class Lexer
{
public:
    explicit Lexer(const std::string input);

public:
    // 获取下一个 Token
    Token next();

    // 不移动位置，查看下一个 Token
    const Token & peek();

    // 是否还有更多 Token
    [[nodiscard]]
    bool has_more() const noexcept;

    // 获取当前 Token 位置
    [[nodiscard]]
    TokenLocation location() const noexcept;

private:
    // 获取下一个 Token
    [[nodiscard]]
    Token next_internal();

    // 跳过空白字符
    void skip_whitespace();

    // 读取标识符或关键字
    Token read_identifier_or_keyword();

    // 读取数字
    Token read_number();

    // 读取字符串
    Token read_string();

    // 检查字符是否是字母
    [[nodiscard]]
    bool is_alpha(char c) const noexcept;

    // 检查字符是否是数字
    bool is_digit(char c) const noexcept;

    // 检查字符是否是字母或数字
    bool is_alnum(char c) const noexcept;

    // 查看当前字符
    [[nodiscard]]
    char current_char() const noexcept;

    // 获取当前字符并移动到下一个
    char advance() noexcept;

    // 检查当前字符是否匹配预期字符，匹配则移动到下一个
    bool match(char expected);

private:
    std::string input_;
    std::size_t position_;
    TokenLocation location_;
    std::optional<Token> peeked_token_; // 预读的 Token
};

} // namespace litedb::core::parser

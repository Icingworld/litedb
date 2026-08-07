#pragma once

#include <string>
#include <optional>

#include "core/parser/token.hpp"

namespace litedb::core::parser
{

/**
 * @brief 词法分析器
 */
class Lexer
{
public:
    explicit Lexer(const std::string input);

public:
    /**
     * @brief 获取下一个 Token
     * @return 下一个 Token
     */
    Token next();

    /**
     * @brief 不移动位置，查看下一个 Token
     * @return 下一个 Token
     */
    const Token & peek();

    /**
     * @brief 是否还有更多 Token
     * @return 是否还有更多 Token
     */
    [[nodiscard]]
    bool has_more() const noexcept;

    /**
     * @brief 获取当前 Token 位置
     * @return 当前 Token 位置
     */
    [[nodiscard]]
    TokenLocation location() const noexcept;

private:
    /**
     * @brief 获取下一个 Token
     * @return 下一个 Token
     */
    [[nodiscard]]
    Token next_internal();

    /**
     * @brief 跳过空白字符
     */
    void skip_whitespace();

    /**
     * @brief 读取标识符或关键字
     * @return 标识符或关键字
     */
    Token read_identifier_or_keyword();

    /**
     * @brief 读取数字
     * @return 数字
     */
    Token read_number();

    /**
     * @brief 读取字符串
     * @return 字符串
     * @note 使用单引号或者双引号包裹
     */
    Token read_string();

    /**
     * @brief 检查字符是否是字母
     * @param c 字符
     * @return 是否是字母
     */
    [[nodiscard]]
    bool is_alpha(char c) const noexcept;

    /**
     * @brief 检查字符是否是数字
     * @param c 字符
     * @return 是否是数字
     */
    bool is_digit(char c) const noexcept;

    /**
     * @brief 检查字符是否是字母或数字
     * @param c 字符
     * @return 是否是字母或数字
     */
    bool is_alnum(char c) const noexcept;

    /**
     * @brief 查看当前字符
     * @return 当前字符
     */
    [[nodiscard]]
    char current_char() const noexcept;

    /**
     * @brief 获取当前字符并移动到下一个
     */
    char advance() noexcept;

    /**
     * @brief 检查当前字符是否匹配预期字符，匹配则移动到下一个
     * @param expected 预期字符
     * @return 是否匹配
     */
    bool match(char expected);

private:
    std::string input_;                     // 原始字符串
    std::size_t position_;                  // 当前字符位置
    TokenLocation location_;                // 当前 Token 位置
    std::optional<Token> peeked_token_;     // 预读的 Token
};

} // namespace litedb::core::parser

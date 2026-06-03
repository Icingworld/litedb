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
     * @brief 获取下一个词法单元
     * @return 下一个词法单元
     */
    Token next();

    /**
     * @brief 不移动位置，查看下一个词法单元
     * @return 下一个词法单元
     */
    const Token & peek();

    /**
     * @brief 是否还有更多词法单元
     * @return 是否还有更多词法单元
     */
    [[nodiscard]]
    bool has_more() const noexcept;

    /**
     * @brief 获取当前词法单元位置
     * @return 当前词法单元位置
     */
    [[nodiscard]]
    TokenLocation location() const noexcept;

private:
    /**
     * @brief 获取下一个词法单元
     * @return 下一个词法单元
     */
    [[nodiscard]]
    Token next_internal();

    /**
     * @brief 跳过空白字符
     */
    void skip_whitespace();

    /**
     * @brief 查看下一个字符
     * @return 下一个字符
     */
    [[nodiscard]]
    char peek_char() const noexcept;

    /**
     * @brief 获取当前字符并移动到下一个
     */
    [[nodiscard]]
    char advance() noexcept;

private:
    std::string input_;                     ///< 原始字符串
    std::size_t position_;                  ///< 当前字符位置
    TokenLocation location_;                ///< 当前词法单元位置
    std::optional<Token> peeked_token_;     ///< 预读的词法单元
};

} // namespace litedb::core::parser
